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
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "receive.h"
#include "utf8.h"
#include "ws.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define CWS_NONCTRL_MASK    (CWS_CONT|CWS_BINARY|CWS_TEXT)

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static size_t _writefunction_cb(const char*, size_t, size_t, void*);
static size_t _cws_process_frame(CWS*, const char*, size_t);
static inline size_t _min_size_t(size_t, size_t);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void receive_init(CWS *priv)
{
    curl_easy_setopt(priv->easy, CURLOPT_WRITEFUNCTION, _writefunction_cb);
    curl_easy_setopt(priv->easy, CURLOPT_WRITEDATA, priv);
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

/**
 * This is the callback used by curl.
 */
static size_t _writefunction_cb(const char *buffer, size_t count, size_t nitems, void *data)
{
    CWS *priv = data;
    size_t len = count * nitems;

    (*priv->debug_fn)(priv, "< websocket bytes received: %ld\n", len);

    if (priv->redirection) {
        (*priv->debug_fn)(priv, "< websocket bytes ignored due to redirection\n");
        return len;
    }

    while (len > 0) {
        size_t used;

        if (priv->closed) {
            return count * nitems;
        }

        used = _cws_process_frame(priv, buffer, len);

        if (0 == used) {
            return count * nitems;
        }

        len -= used;
        buffer += used;
    }

    return count * nitems;
}


/**
 * A helper function that processes the data into a frame if possible.
 *
 * @param priv the object to reference
 * @param buf  the buffer to work with.  On success, the buffer pointer is
 *             changed to reflect the consumed bytes.
 * @param len  the length of the buffer.  On success, the buffer length is
 *             changed to reflect the consumed bytes.
 *
 * @retval greater than 0 if more bytes are needed
 * @retval 0 if the header is valid
 * @retval -1 if there was an error, the buf & length are not changed & the
 *         socket has been closed automatically.
 */
static ssize_t _process_frame_header(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    struct header *h = &priv->recv.header;
    const char *buffer = *buf;
    size_t _len = *len;
    size_t min;
    ssize_t delta;

    if (r->is_frame_valid) {
        return 0;
    }

    if (0 == h->needed) {
        h->needed = WS_FRAME_HEADER_MIN;
    }

    min = _min_size_t(h->needed, _len);

    memcpy(&h->buf[h->used], buffer, min);
    h->used += min;
    h->needed -= min;
    _len -= min;
    buffer += min;

    delta = 0;
    if (frame_decode(&r->frame, h->buf, h->used, &delta)) {
        cws_close(priv, 1002, NULL, 0);
        return -1;
    }

    /* We need more data for a complete header */
    if (delta < 0) {
        /* Delta is the number of bytes need * -1 */
        h->needed = 0 - delta;
        *buf = buffer;
        *len = _len;
        return h->needed;
    }

    if (0 != frame_validate(&r->frame, FRAME_DIR_S2C)) {
        cws_close(priv, 1002, NULL, 0);
        return -1;
    }

    /* Done processing the buffer because we have all the data we need. */
    h->used = 0;
    r->is_frame_valid = true;
    *buf = buffer;
    *len = _len;

    return 0;
}


static bool _is_valid_close_from_server(int code)
{
    if (((3000 <= code) && (code <= 4999)) ||
        ((1000 <= code) && (code <= 1003)) ||
        ((1007 <= code) && (code <= 1011)))
    {
        return true;
    }

    return false;
}


static void _handle_close(CWS *priv, struct recv *r)
{
    int status = 1005;
    const char *s = NULL;
    size_t len = r->frame.payload_len;

    if (1 == len) {
        /* Invalid as 0 or 2+ bytes are needed */
        status = 1002;
        len = 0;
        cws_close(priv, status, "invalid close payload length", SIZE_MAX);
        return;
    }

    if (1 < len) {
        ssize_t rv;

        status = r->control.buf[0] << 8 | r->control.buf[1];
        if (!_is_valid_close_from_server(status)) {
            status = 1002;
            len = 0;
            cws_close(priv, status, "invalid close reason", SIZE_MAX);
            return;
        }

        /* Ensure there is a trailing '\0' to prevent buffer overruns */
        r->control.buf[r->frame.payload_len] = '\0';
        s = (const char*) &r->control.buf[2];
        len -= 2;

        rv = utf8_validate(s, len);
        if (len != (size_t) rv) {
            cws_close(priv, 1007, NULL, 0);
            return;
        }
    }

    (priv->on_close_fn)(priv->user, priv, status, s, len);

    if (!priv->closed) {
        if (1005 == status) {
            status = 0;
        }
        cws_close(priv, status, s, 0);
    }
}


/**
 * @retval  greater than 0 if more bytes are needed
 * @retval  0 if the control frame was processed
 * @retval -1 if there was an error & the frame was closed
 */
static ssize_t _process_control_frame(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    const char *buffer = *buf;
    size_t _len = *len;

    if ((r->control.used < r->frame.payload_len) && (0 < _len)) {
        size_t min = _min_size_t(r->frame.payload_len - r->control.used, _len);

        memcpy(&r->control.buf[r->control.used], buffer, min);
        r->control.used += min;
        _len -= min;
        buffer += min;
    }

    if (r->control.used == r->frame.payload_len) {
        if (WS_OPCODE_PING == r->frame.opcode) {
            priv->dispatching++;
            (priv->on_ping_fn)(priv->user, priv, r->control.buf, r->control.used);
            priv->dispatching--;
            r->is_frame_valid = false;
        } else if (WS_OPCODE_PONG == priv->recv.frame.opcode) {
            priv->dispatching++;
            (priv->on_pong_fn)(priv->user, priv, r->control.buf, r->control.used);
            priv->dispatching--;
            r->is_frame_valid = false;
        } else {    /* We know the frame only has these 3 */
            _handle_close(priv, r);
            r->is_frame_valid = false;
            return -1;
        }

        r->header.needed = WS_FRAME_HEADER_MIN;
        r->header.used = 0;
        r->control.used = 0;
    }

    *len = _len;
    *buf = buffer;
    return 0;
}


/**
 * @retval  greater than 0 if more bytes are needed
 * @retval  0 if the data was processed
 * @retval -1 if there was an error & the frame was closed
 */
static ssize_t _process_frame_data_stream(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    const char *buffer = *buf;
    const char *buf_to_send = buffer;
    size_t _len = *len;
    size_t min = _min_size_t(r->frame.payload_len, _len);
    size_t len_to_send = min;

    if (0 == r->stream_type) {
        /* 1st stream packet wasn't a continuation */
        cws_close(priv, 1002, NULL, 0);
        return -1;
    }

    if ((CWS_TEXT == r->stream_type) && (0 < r->utf8.needed)) {
        min = _min_size_t(r->utf8.needed, min);
        memcpy(&r->utf8.buf[r->utf8.used], buffer, min);
        r->utf8.used += min;
        r->utf8.needed -= min;

        if (0 != r->utf8.needed) {
            if (false == utf8_maybe_valid(r->utf8.buf, r->utf8.used)) {
                cws_close(priv, 1007, NULL, 0);
                return -1;
            }

            len_to_send = min;
            buf_to_send = NULL;
            /*
            r->frame.payload_len -= min;

            buffer += min;
            _len -= min;
            *buf = buffer;
            *len = _len;
            return r->utf8.needed;
            */
        } else {
            ssize_t rv;

            /* We have extra bytes from the previous frame, so we should add
             * them to this frame to make sure we don't roll over backwards. */
            r->frame.payload_len += r->utf8.used - min;

            len_to_send = r->utf8.used;
            buf_to_send = r->utf8.buf;

            rv = utf8_validate(buf_to_send, len_to_send);
            if (rv < 0) {
                cws_close(priv, 1007, NULL, 0);
                return -1;
            }
        }
    } else if (CWS_TEXT == r->stream_type) {
        ssize_t rv;
        size_t left;

        rv = utf8_validate(buffer, min);
        left = min - (size_t) rv;

        if (rv < 0) {
            cws_close(priv, 1007, NULL, 0);
            return -1;
        }

        if (0 < left) {
            if (0 == r->frame.fin) {
                memcpy(r->utf8.buf, &buffer[min - (size_t) left], left);
                r->utf8.used = left;
                r->utf8.needed = utf8_get_size(r->utf8.buf[0]);
                r->utf8.needed -= left;

                r->frame.payload_len -= left;
                len_to_send -= left;
            } else {
                cws_close(priv, 1007, NULL, 0);
                return -1;
            }
        }
    }

    r->frame.payload_len -= len_to_send;

    if ((0 == r->frame.payload_len) && (1 == r->frame.fin)) {
        r->fragment_info |= CWS_LAST;
    }

    if (NULL != buf_to_send) {
        priv->dispatching++;
        (priv->on_stream_fn)(priv->user, priv, r->fragment_info, buf_to_send, len_to_send);
        priv->dispatching--;
    }

    r->fragment_info &= ~CWS_FIRST;

    if (buf_to_send == r->utf8.buf) {
        r->utf8.used = 0;
        r->utf8.needed = 0;
    }

    if (0 == r->frame.payload_len) {
        r->is_frame_valid = false;
        /* Set the expectation for the next frame to be a CONTINUATION */
        r->fragment_info &= ~CWS_NONCTRL_MASK;
        r->fragment_info |= CWS_CONT;
    }

    if (CWS_LAST == (CWS_LAST & r->fragment_info)) {
        r->fragment_info = 0;
        r->stream_type = 0;
    }

    buffer += min;
    _len -= min;


    *buf = buffer;
    *len = _len;
    return 0;
}

/**
 * @retval  greater than 0 if more bytes are needed
 * @retval  0 if the control frame was processed
 * @retval -1 if there was an error & the frame was closed
 */
static ssize_t _process_data_frame(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    const char *buffer = *buf;
    size_t _len = *len;
    ssize_t rv;

    if (WS_OPCODE_BINARY == r->frame.opcode) {
        if (0 == r->fragment_info) {
            /* New frame */
            r->stream_type = CWS_BINARY;
            r->fragment_info = CWS_FIRST | CWS_BINARY;
        } else if ((0 < r->frame.payload_len) &&
                   (CWS_BINARY == (CWS_NONCTRL_MASK & r->fragment_info)))
        {
            /* Continue to send data */
        } else {
            cws_close(priv, 1002, NULL, 0);
            return -1;
        }
    } else if (WS_OPCODE_TEXT == r->frame.opcode) {
        if (0 == r->fragment_info) {
            /* New frame */
            r->stream_type = CWS_TEXT;
            r->fragment_info = CWS_FIRST | CWS_TEXT;
        } else if ((0 < r->frame.payload_len) &&
                   (CWS_TEXT == (CWS_NONCTRL_MASK & r->fragment_info)))
        {
            /* Continue to send data */
        } else {
            cws_close(priv, 1002, NULL, 0);
            return -1;
        }
    } else if (WS_OPCODE_CONTINUATION == r->frame.opcode) {
        if (0 == r->stream_type) {
            cws_close(priv, 1002, NULL, 0);
            return -1;
        }
        r->fragment_info &= ~CWS_NONCTRL_MASK;
        r->fragment_info |= CWS_CONT;
    } else {
        /* Should never be hit unless the spec changes and only have of this
         * implementation changes. */
        cws_close(priv, 1002, NULL, 0);
        return -1;
    }

    rv = _process_frame_data_stream(priv, &buffer, &_len);

    *buf = buffer;
    *len = _len;
    return rv;
}


static size_t _cws_process_frame(CWS *priv, const char *buffer, size_t len)
{
    size_t starting_len = len;

    /* Are we waiting on frame header data? */
    if (0 != _process_frame_header(priv, &buffer, &len)) {
        return starting_len - len;
    }

    /* There is a completed header */
    if (priv->recv.frame.is_control) {
        _process_control_frame(priv, &buffer, &len);
    } else {
        _process_data_frame(priv, &buffer, &len);
    }

    return starting_len - len;
}



static inline size_t _min_size_t(size_t a, size_t b)
{
    return (a < b) ? a : b;
}
