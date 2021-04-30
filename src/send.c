/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

#include "internal.h"
#include "frame.h"
#include "send.h"
#include "verbose.h"

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
    bool is_close_frame;
    size_t len;
    size_t written;
    size_t sent;
    uint8_t buffer[];
};

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static size_t _send_cb(char*, size_t, size_t, void*);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

CURLcode send_init(CWS *priv)
{
    CURLcode rv;

    rv  = curl_easy_setopt(priv->easy, CURLOPT_READFUNCTION, _send_cb);
    rv |= curl_easy_setopt(priv->easy, CURLOPT_READDATA, priv);

    return rv;
}


void send_destroy(CWS *priv)
{
    while (priv->send) {
        struct cws_buf_queue *tmp;

        tmp = priv->send->next;
        mem_free(priv->send);
        priv->send = tmp;
    }
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
        buffer_size = priv->cfg.max_payload_size + WS_FRAME_HEADER_MAX;
    }

    if (!buf) {
        return CWSE_OUT_OF_MEMORY;
    }

    memset(buf, 0, sizeof(struct cws_buf_queue));
    buf->len = buffer_size;

    buf->written = frame_encode(f, buf->buffer, buffer_size);

    /* We need to handle closes specially, so mark the packet. */
    if (WS_OPCODE_CLOSE == f->opcode) {
        buf->is_close_frame = true;
    }

    /* Queue the buffer and start sending if not already. */
    if (NULL == priv->send) {
        priv->send = buf;
    } else {
        struct cws_buf_queue *qP = priv->send;

        if (f->is_urgent) {
            if (0 < qP->sent) {
                /* This frame is partially sent.  Add the urgent frame next. */
                buf->next = qP->next;
                buf->prev = qP;
                qP->next = buf;
            } else {
                /* Nothing sent.  Insert the urgent frame before the next frame. */
                priv->send = buf;
                buf->next = qP;
                qP->prev = buf;
            }
        } else {
            while (qP->next) {
                qP = qP->next;
            }
            buf->prev = qP;
            qP->next = buf;
        }
    }

    verbose(priv, "[ websocket frame queued opcode: %s payload len: %zd ]\n",
            frame_opcode_to_string(f), f->payload_len);

    /* Start the sending process from curl */
    if (priv->pause_flags & CURLPAUSE_SEND) {

        priv->pause_flags &= ~CURLPAUSE_SEND;
        curl_easy_pause(priv->easy, priv->pause_flags);

        verbose(priv, "[ websocket unpause sending ]\n");
    }
    return CWSE_OK;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/


/**
 * Fills the specified buffer with as much data as is available.
 *
 * @param priv   the curlws object of reference
 * @param buffer the buffer to fill
 * @param len    the number of bytes available in the buffer
 *
 * @return the number of bytes written into the buffer
 */
static size_t _fill_outgoing_buffer(CWS *priv, char *buffer, size_t len)
{
    size_t data_to_send;
    size_t sent = 0;

    data_to_send = priv->send->written - priv->send->sent;

    /* Fill up the buffer with whatever frames we have queued. */
    while ((0 < len) && (0 < data_to_send)) {
        const uint8_t *p;
        size_t lesser = len;
        p = &priv->send->buffer[priv->send->sent];

        if (data_to_send < lesser) {
            lesser = data_to_send;
        }
        memcpy(buffer, p, lesser);

        buffer += lesser;
        sent += lesser;
        len -= lesser;
        data_to_send -= lesser;
        priv->send->sent += lesser;

        /* If we've sent a buffer, recycle it. */
        if (priv->send->sent == priv->send->written) {
            struct cws_buf_queue *tmp = priv->send;
            bool is_close = tmp->is_close_frame;

            priv->send = priv->send->next;
            if (priv->send) {
                priv->send->prev = NULL;
                data_to_send += priv->send->written;
            }
            mem_free(tmp);

            /* Don't send any more data after we send a close frame. */
            if (is_close) {
                priv->close_state |= CLOSE_SENT;
                verbose_close(priv);
                data_to_send = 0;
                send_destroy(priv);
            }
        }
    }

    return sent;
}


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
static size_t _send_cb(char *buffer, size_t count, size_t n, void *data)
{
    CWS *priv = data;
    size_t len = count * n;
    size_t sent = 0;

    if (priv->header_state.redirection) {
        verbose(priv, "> websocket %zd bytes ignored due to redirection\n", len);
        return len;
    }

    if (NULL == priv->send) {
        /* When the connection is closed, we should return 0 to tell curl to
         * shut down the connection. */
        if (READY_TO_CLOSE(priv->close_state)) {
            priv->close_state |= CLOSED;
            verbose_close(priv);
            verbose(priv, "> websocket closed by returning 0\n");
            return 0;
        }

        priv->pause_flags |= CURLPAUSE_SEND;

        verbose(priv, "> websocket sending paused\n");

        return CURL_READFUNC_PAUSE;
    }

    sent = _fill_outgoing_buffer(priv, buffer, len);

    verbose(priv, "> websocket sent: %ld\n", sent);

    return sent;
}
