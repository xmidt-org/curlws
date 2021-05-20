/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __CWS_INTERNAL_H__
#define __CWS_INTERNAL_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "frame.h"
#include "memory.h"
#include "utf8.h"
#include "ws.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define IGNORE_UNUSED(x) (void) x

#define CLOSE_RECEIVED      0x0010
#define CLOSE_QUEUED        0x0020
#define CLOSE_SENT          0x0040
#define CLOSED              0x0080
#define READY_TO_CLOSE(x)   \
    ((CLOSE_SENT|CLOSE_RECEIVED) == ((CLOSED|CLOSE_SENT|CLOSE_RECEIVED) & x))

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/

struct cfg_set {
    /* The initial URL to connect to. */
    char *url;

    /* The user provided data. */
    void *user;

    /* The maximum payload size. */
    size_t max_payload_size;

    /* The verbosity of the logging. */
    int verbose;

    /* The stream to log to. */
    FILE *verbose_stream;

    /* The request websocket protocols. */
    char *ws_protocols_requested;

    bool follow_redirects;
};

/* The callback functions used.  These will be called without NULL
 * checking as the library expects to use default values. */
struct callbacks {
    int (*on_connect_fn)(void*, CWS*, const char*);
    int (*on_text_fn)(void*, CWS*, const char*, size_t);
    int (*on_binary_fn)(void*, CWS*, const void*, size_t);
    int (*on_fragment_fn)(void*, CWS*, int, const void*, size_t);
    int (*on_ping_fn)(void*, CWS*, const void*, size_t);
    int (*on_pong_fn)(void*, CWS*, const void*, size_t);
    int (*on_close_fn)(void*, CWS*, int, const char*, size_t);
};

struct recv {
    /* The information needed across fragments */
    int stream_type;
    int fragment_info;

    struct utf8_buffer {    
        char buf[MAX_UTF_BYTES];
        size_t used;
        size_t needed;
    } utf8;

    /* If the decoded frame is valid, it is not NULL. */
    struct cws_frame *frame;

    /* This is the actual frame data pointed to by frame.  This saves the
     * overhead of allocation/free. */
    struct cws_frame _frame;

    /* Scratch space for collecting the data needed to decode WS a frame
     * header. */
    struct header {
        uint8_t buf[WS_FRAME_HEADER_MAX];
        size_t used;
        size_t needed;
    } header;

    /* Scratch space for collecting the data needed to decode control WS
     * payload.  The header is still stored in the header struct above. */
    struct control {
        uint8_t buf[WS_CTL_PAYLOAD_MAX];
        size_t used;
        size_t needed;
    } control;
};

struct header_map {
    bool redirection;
    bool accepted;
    bool upgraded;
    bool connection_websocket;

    /* The websocket extensions */
    char *ws_protocols_received;
};

struct cws_object {
    /* Configured values that shouldn't change after they are set. */
    struct cfg_set cfg;

    /* The callbacks. */
    struct callbacks cb;

    /* The curl object that represents the connection. */
    CURL *easy;

    /* The headers curl needs us to store. */
    struct curl_slist *headers;

    /* The key header that the server is expected to return. */
    char *expected_key_header;
    size_t expected_key_header_len;

    /* The memory configuration & pool. */
    struct mem_pool_config mem_cfg;
    pool_t *mem;

    /* The details about the last data frame queued to send. */
    int last_sent_data_frame_info;

    /* The outgoing queue of bytes to send. */
    struct cws_buf_queue *send;

    /* The structure needed to deal with the incoming data stream */
    struct recv recv;

    int stream_type;
    size_t stream_buffer_len;
    void *stream_buffer;

    /* Connection State flags */
    uint8_t dispatching;
    int pause_flags;

    /* The header state structure */
    struct header_map header_state;

    int close_state;
};

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
/* none */

#endif

