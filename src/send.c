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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "frame.h"
#include "send.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
struct cws_buf_queue {
    struct cws_buf_queue *prev;
    struct cws_buf_queue *next;
    size_t len;
    size_t written;
    size_t sent;
    uint8_t buffer[];
};

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static size_t _readfunction_cb(char*, size_t, size_t, void*);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

void send_init(CWS *priv)
{
    curl_easy_setopt(priv->easy, CURLOPT_READFUNCTION, _readfunction_cb);
    curl_easy_setopt(priv->easy, CURLOPT_READDATA, priv);
}

size_t send_get_memory_needed(size_t payload_size)
{
    return sizeof(struct cws_buf_queue) + payload_size;
}


CWScode send_frame(CWS *priv, const struct cws_frame *f)
{
    struct cws_buf_queue *buf;
    size_t buffer_size;

    if (f->is_control) {
        buf = (struct cws_buf_queue*) mem_alloc_ctrl(priv->mem);
        buffer_size = WS_CTL_FRAME_MAX;
    } else {
        buf = (struct cws_buf_queue*) mem_alloc_data(priv->mem);
        buffer_size = priv->max_payload_size + WS_FRAME_HEADER_MAX;
    }

    if (!buf) {
        return CWSE_OUT_OF_MEMORY;
    }

    memset(buf, 0, sizeof(struct cws_buf_queue));
    buf->len = buffer_size;

    buf->written = frame_encode(f, buf->buffer, buffer_size);

    /* Queue the buffer and start sending if not already. */
    if (NULL == priv->send) {
        priv->send = buf;
    } else {
        struct cws_buf_queue *qP = priv->send;

        while (qP->next) {
            qP = qP->next;
        }
        buf->prev = qP;
        qP->next = buf;
    }

    /* Start the sending process from curl */
    if (priv->pause_flags & CURLPAUSE_SEND) {
        priv->pause_flags &= ~CURLPAUSE_SEND;
        curl_easy_pause(priv->easy, priv->pause_flags);
    }
    return CWSE_OK;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

/**
 * The function provided to curl when it wants to read data from this library.
 *
 * @param buffer the buffer to put the data in
 * @param count  the count of n
 * @param n      the size of a byte (always 1, per curl docs)
 * @param data   the user specified data
 *
 * @return the number of bytes provided or CURL_READFUNC_PAUSE
 */
static size_t _readfunction_cb(char *buffer, size_t count, size_t n, void *data)
{
    CWS *priv = data;
    size_t space_left = count * n;
    size_t sent = 0;
    size_t data_to_send;

    printf( "_readfunction_cb()\n");
    if (priv->redirection) {
        printf( "_readfunction_cb() exit: %ld\n", space_left);
        return space_left;
    }

    if (NULL == priv->send) {
        priv->pause_flags |= CURLPAUSE_SEND;
        printf( "_readfunction_cb() exit: PAUSE\n");
        return CURL_READFUNC_PAUSE;
    }

    data_to_send = priv->send->written - priv->send->sent;

    /* Fill up the buffer with whatever frames we have queued. */
    while ((0 < space_left) && (0 < data_to_send)) {
        uint8_t *p;
        size_t lesser = space_left;
        p = &priv->send->buffer[priv->send->sent];

        if (data_to_send < lesser) {
            lesser = data_to_send;
        }
        memcpy(buffer, p, lesser);

        buffer += lesser;
        sent += lesser;
        space_left -= lesser;
        data_to_send -= lesser;
        priv->send->sent += lesser;

        /* If we've sent a buffer, recycle it. */
        if (priv->send->sent == priv->send->written) {
            struct cws_buf_queue *tmp = priv->send;

            priv->send = priv->send->next;
            if (priv->send) {
                priv->send->prev = NULL;
                data_to_send += priv->send->written;
            }
            mem_free(tmp);
        }
    }

    printf( "_readfunction_cb() exit: %ld\n", sent);
    return sent;
}