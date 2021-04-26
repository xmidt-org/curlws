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

#include "cb.h"
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
static void _cws_process_frame(CWS*, const char**, size_t*);
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

    if (priv->cfg.verbose) {
        fprintf(priv->cfg.verbose_stream, "< websocket bytes received: %ld\n", len);
    }

    if (priv->header_state.redirection) {
        if (priv->cfg.verbose) {
            fprintf(priv->cfg.verbose_stream, "< websocket bytes ignored due to redirection\n");
        }
        return len;
    }

    while (len > 0) {
        size_t prev_len = len;

        if (priv->closed) {
            return count * nitems;
        }

        _cws_process_frame(priv, &buffer, &len);

        if (len == prev_len) {
            return count * nitems;
        }
    }

    return count * nitems;
}


static void _handle_close(CWS *priv, struct recv *r)
{
    int status = 1005;
    const char *s = NULL;
    size_t len = r->frame->payload_len;

    if (1 == len) {
        /* Invalid as 0 or 2+ bytes are needed */
        status = 1002;
        cws_close(priv, status, "invalid close payload length", SIZE_MAX);
        return;
    }

    if (1 < len) {
        size_t prev_len;

        status = r->control.buf[0] << 8 | r->control.buf[1];
        if (!is_close_code_valid(status)) {
            status = 1002;
            cws_close(priv, status, "invalid close reason", SIZE_MAX);
            return;
        }

        /* Ensure there is a trailing '\0' to prevent buffer overruns */
        r->control.buf[r->frame->payload_len] = '\0';
        s = (const char*) &r->control.buf[2];
        len -= 2;

        prev_len = len;
        if ((0 != utf8_validate(s, &len)) || (prev_len != len)) {
            cws_close(priv, 1007, NULL, 0);
            return;
        }
    }

    cb_on_close(priv, status, s, len);

    if (!priv->closed) {
        if (1005 == status) {
            status = 0;
        }
        cws_close(priv, status, s, 0);
    }
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
static struct cws_frame* _process_frame_header(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    struct header *h = &priv->recv.header;
    const char *buffer = *buf;
    size_t _len = *len;
    size_t min;
    long delta;
    struct cws_frame *frame;

    if (0 == h->needed) {
        h->needed = WS_FRAME_HEADER_MIN;
    }

    min = _min_size_t(h->needed, _len);

    memcpy(&h->buf[h->used], buffer, min);
    h->used += min;
    h->needed -= min;
    _len -= min;
    buffer += min;

    frame = &r->_frame;

    delta = 0;
    if (frame_decode(frame, h->buf, h->used, &delta)) {
        cws_close(priv, 1002, NULL, 0);
        return NULL;
    }

    /* We need more data for a complete header */
    if (delta < 0) {
        /* Delta is the number of bytes need * -1 */
        h->needed = 0 - delta;
        *buf = buffer;
        *len = _len;
        return NULL;
    }

    if (0 != frame_validate(frame, FRAME_DIR_S2C)) {
        cws_close(priv, 1002, NULL, 0);
        return NULL;
    }

    /* Done processing the buffer because we have all the data we need. */
    h->used = 0;
    *buf = buffer;
    *len = _len;

    return frame;
}


/**
 * @retval  greater than 0 if more bytes are needed
 * @retval  0 if the control frame was processed
 * @retval -1 if there was an error & the frame was closed
 */
static void _process_control_frame(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    const char *buffer = *buf;
    size_t _len = *len;

    if ((r->control.used < r->frame->payload_len) && (0 < _len)) {
        size_t min = _min_size_t(r->frame->payload_len - r->control.used, _len);

        memcpy(&r->control.buf[r->control.used], buffer, min);
        r->control.used += min;
        _len -= min;
        buffer += min;
    }

    if (r->control.used == r->frame->payload_len) {
        if (WS_OPCODE_PING == r->frame->opcode) {
            priv->dispatching++;
            cb_on_ping(priv, r->control.buf, r->control.used);
            priv->dispatching--;
            r->frame = NULL;
        } else if (WS_OPCODE_PONG == priv->recv.frame->opcode) {
            priv->dispatching++;
            cb_on_pong(priv, r->control.buf, r->control.used);
            priv->dispatching--;
            r->frame = NULL;
        } else {    /* We know the frame only has these 3 */
            _handle_close(priv, r);
            r->frame = NULL;
            *len = _len;
            *buf = buffer;
            return;
        }

        r->header.needed = WS_FRAME_HEADER_MIN;
        r->header.used = 0;
        r->control.used = 0;
    }

    *len = _len;
    *buf = buffer;
}

static int _process_text_stream(CWS *priv, const char *buf, size_t len,
                                const char **buf_to_send, size_t *len_to_send)
{
    struct recv *r = &priv->recv;

    *len_to_send = len;
    if (0 < r->utf8.needed) {
        len = _min_size_t(r->utf8.needed, len);
        memcpy(&r->utf8.buf[r->utf8.used], buf, len);
        r->utf8.used += len;
        r->utf8.needed -= len;

        if (0 != r->utf8.needed) {
            /* Fail as soon as we find a UTF8 sequence that isn't valid vs.
             * waiting for the entire character.  This makes us autobahn
             * compliant. */
            if (false == utf8_maybe_valid(r->utf8.buf, r->utf8.used)) {
                cws_close(priv, 1007, NULL, 0);
                return -1;
            }

            *buf_to_send = NULL;
            *len_to_send = 0;
        } else {
            size_t valid;

            valid = r->utf8.used;
            if ((0 != utf8_validate(r->utf8.buf, &valid)) ||
                (r->utf8.used != valid))
            {
                cws_close(priv, 1007, NULL, 0);
                return -1;
            }

            /* Consume the utf8 buffer as well as the characters that came in
             * this call and get ready for the next block of data. */
            *buf_to_send = r->utf8.buf;
            *len_to_send = r->utf8.used;
            r->utf8.used = 0;
        }
    } else {
        size_t valid;

        valid = len;
        /* This code block handles the cases where there are no outstanding UTF8
         * bytes needed to complete a character at the start.  There could be
         * incomplete UTF8 characters at the end. */
        if (0 != utf8_validate(*buf_to_send, &valid)) {
            cws_close(priv, 1007, NULL, 0);
            return -1;
        }

        /* Below moves any extra bytes we have that are probably incomplete UTF8
         * characters into the buffer set aside to deal with that. */
        if (valid < len) {
            size_t left = len - valid;

            /* If this is the last buffer of the last frame and we have extra
             * characters, then we have a problem and must close error out. */
            if ((r->frame->fin) && (r->frame->payload_len <= len)) {
                cws_close(priv, 1007, NULL, 0);
                return -1;
            }

            memcpy(r->utf8.buf, &buf[valid], left);
            r->utf8.used = left;

            /* The first byte of a UTF8 character is enough to determine the
             * length needed for the complete character. */
            r->utf8.needed = utf8_get_size(r->utf8.buf[0]);
            r->utf8.needed -= left;

            *len_to_send = valid;
        }
    }

    return 0;
}

static void _send_data_frame(CWS *priv, const char *buffer, size_t len)
{
    struct recv *r = &priv->recv;

    /* If this is the last byte of the last frame, mark the fragment as LAST */
    if ((0 == r->frame->payload_len) && (r->frame->fin)) {
        r->fragment_info |= CWS_LAST;
    }

    /* Clean up the buffer & length arguments. */
    if (buffer) {
        if (0 == len) {
            buffer = NULL;
        }
    } else {
        len = 0;
    }

    /* Send the buffer if we have data or if this is the first or last frame.
     * Doing a bit of filtering here makes testing a bit simpler and consistent
     * as well as it cuts down on sending a bunch of useless empty buffers. */
    if ((0 < len) || ((CWS_FIRST | CWS_LAST) & r->fragment_info)) {
        priv->dispatching++;
        cb_on_fragment(priv, r->fragment_info, buffer, len);
        priv->dispatching--;
    }

    /* Set the expectation for the next frame to be a CONTINUATION */
    r->fragment_info &= ~CWS_NONCTRL_MASK;
    r->fragment_info |= CWS_CONT;

    /* The next chuck of data is definitely not the 'first'. */
    r->fragment_info &= ~CWS_FIRST;

    /* Throw away the frame if we sent all the data associated with it. */
    if (0 == r->frame->payload_len) {
        r->frame = NULL;
    }

    /* If this was the last frame reset the stream markers to expect a TEXT or
     * BINARY frame to lead off a new stream. */
    if (CWS_LAST & r->fragment_info) {
        r->fragment_info = 0;
        r->stream_type = 0;
    }
}


/**
 * @retval  greater than 0 if more bytes are needed
 * @retval  0 if the data was processed
 * @retval -1 if there was an error & the frame was closed
 */
static void _process_data_frame(CWS *priv, const char **buf, size_t *len)
{
    struct recv *r = &priv->recv;
    const char *buf_to_send = *buf;
    size_t min = _min_size_t(r->frame->payload_len, *len);
    size_t len_to_send = min;

    if (CWS_TEXT == r->stream_type) {
        int rv;

        rv = _process_text_stream(priv, *buf, min, &buf_to_send, &len_to_send);
        if (rv < 0) {
            return;
        }
    }
    r->frame->payload_len -= min;

    _send_data_frame(priv, buf_to_send, len_to_send);

    *buf += min;
    *len -= min;
}


static void _cws_process_frame(CWS *priv, const char **buffer, size_t *len)
{
    struct cws_frame *frame = priv->recv.frame;

    if (!frame) {
        struct recv *r = &priv->recv;

        /* Are we waiting on frame header data? */
        frame = _process_frame_header(priv, buffer, len);

        if (frame && (0 == frame->is_control)) {
            if ((0 == r->fragment_info) && (WS_OPCODE_BINARY == frame->opcode)) {
                r->stream_type = CWS_BINARY;
                r->fragment_info = CWS_FIRST | CWS_BINARY;
            } else if ((0 == r->fragment_info) && (WS_OPCODE_TEXT == frame->opcode)) {
                r->stream_type = CWS_TEXT;
                r->fragment_info = CWS_FIRST | CWS_TEXT;
            } else if ((0 != r->stream_type) && (WS_OPCODE_CONTINUATION == frame->opcode)) {
                r->fragment_info &= ~CWS_NONCTRL_MASK;
                r->fragment_info |= CWS_CONT;
            } else {
                cws_close(priv, 1002, NULL, 0);
                return;
            }
        }
        priv->recv.frame = frame;
    }

    if (frame) {
        /* There is a completed header */
        if (frame->is_control) {
            _process_control_frame(priv, buffer, len);
        } else {
            _process_data_frame(priv, buffer, len);
        }
    }
}



static inline size_t _min_size_t(size_t a, size_t b)
{
    return (a < b) ? a : b;
}
