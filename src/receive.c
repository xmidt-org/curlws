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
#include "ws.h"

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
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static size_t _writefunction_cb(const char*, size_t, size_t, void*);
static size_t _cws_process_frame(CWS*, const char*, size_t);
static void _handle_control_frame(CWS*);
static inline size_t _min_size_t(size_t, size_t);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void receive_init(CWS *priv)
{
    priv->recv.header.needed = WS_FRAME_HEADER_MIN;

    curl_easy_setopt(priv->easy, CURLOPT_WRITEFUNCTION, _writefunction_cb);
    curl_easy_setopt(priv->easy, CURLOPT_WRITEDATA, priv);
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static size_t _writefunction_cb(const char *buffer, size_t count, size_t nitems, void *data)
{
    CWS *priv = data;
    size_t len = count * nitems;

    printf("_writefuncation_cb( buffer, %ld, %ld, data )\n", count, nitems);

    if (priv->redirection) {
        printf("_writefuncation_cb( buffer, %ld, %ld, data ) Redirection, Exit\n", count, nitems);
        return len;
    }

    while (len > 0) {
        size_t used;

        if (priv->closed) {
            printf("_writefuncation_cb( buffer, %ld, %ld, data ) Closed, Exit\n", count, nitems);
            return count * nitems;
        }

        used = _cws_process_frame(priv, buffer, len);

        if (0 == used) {
            printf("_writefuncation_cb( buffer, %ld, %ld, data ) Error, Exit\n", count, nitems);
            return 0;
        }

        len -= used;
        buffer += used;
    }

    printf("_writefuncation_cb( buffer, %ld, %ld, data ) Normal, Exit\n", count, nitems);
    return count * nitems;
}


static size_t _cws_process_frame(CWS *priv, const char *buffer, size_t len)
{
    size_t used = 0;

    /* Are we waiting on frame header data? */
    if (priv->recv.header.needed) {
        size_t min = _min_size_t(priv->recv.header.needed, len);
        int rv;
        ssize_t delta;

        memcpy(&priv->recv.header.buf[priv->recv.header.used], buffer, min);
        priv->recv.header.used += min;
        priv->recv.header.needed -= min;
        len -= min;
        used += min;
        buffer += min;

        delta = 0;
        rv = frame_decode(&priv->recv.frame, priv->recv.header.buf, priv->recv.header.used, &delta);
        if (rv) {
            cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, NULL, 0);
            return 0; /* Signal an error with 0. */
        }

        /* We need more data for a complete header */
        if (delta < 0) {
            /* Delta is the number of bytes need * -1 */
            priv->recv.header.needed = 0 - delta;
            return used;
        }
    }

    /* There is a complete header */

    if (priv->recv.frame.is_control) {
        if (priv->recv.control.used < priv->recv.frame.payload_len) {
            size_t min = _min_size_t(priv->recv.frame.payload_len, len);

            memcpy(&priv->recv.control.buf[priv->recv.control.used], buffer, min);
            priv->recv.control.used += min;
            used += min;
        }

        if (priv->recv.control.used == priv->recv.frame.payload_len) {
            _handle_control_frame(priv);

            priv->recv.header.needed = WS_FRAME_HEADER_MIN;
            priv->recv.header.used = 0;
            priv->recv.control.used = 0;
        }

        return used;
    } else {
        size_t min = _min_size_t(priv->recv.frame.payload_len, len);
        used += min;

        if (WS_OPCODE_CONTINUATION == priv->recv.frame.opcode) {
            priv->recv.fragment_info &= ~CWS_FIRST_FRAME;
        } else {
            if (WS_OPCODE_TEXT == priv->recv.frame.opcode) {
                priv->recv.fragment_info = CWS_FIRST_FRAME | CWS_TEXT;
            } else {
                priv->recv.fragment_info = CWS_FIRST_FRAME | CWS_BINARY;
            }
        }

        /* If the payload == the min, then we have the last of the data */
        if (priv->recv.frame.payload_len == min) {
            if (priv->recv.frame.fin) {
                priv->recv.fragment_info |= CWS_LAST_FRAME;
            }

            priv->dispatching++;
            (priv->on_stream_fn)(priv->user, priv, priv->recv.fragment_info, buffer, min);
            priv->dispatching--;

            priv->recv.header.needed = WS_FRAME_HEADER_MIN;
        } else {
            if (0 < min) {
                priv->dispatching++;
                (priv->on_stream_fn)(priv->user, priv, priv->recv.fragment_info, buffer, min);
                priv->dispatching--;

                priv->recv.frame.payload_len -= min;
                priv->recv.frame.opcode = WS_OPCODE_CONTINUATION;
                priv->recv.fragment_info &= ~(CWS_TEXT | CWS_BINARY);
                priv->recv.fragment_info |= CWS_CONT;
            }
        }
    }

    return used;
}

static bool _is_valid_close_from_server(int code)
{
    if (((3000 <= code) && (code <= 4999)) ||
        ((1000 <= code) && (code <= 1003)) ||
        ((1007 <= code) && (code <= 1009)) || (1011 == code))
    {
        return true;
    }

    return false;
}

static void _handle_control_frame(CWS *priv)
{
    if (WS_OPCODE_PING == priv->recv.frame.opcode) {
        priv->dispatching++;
        (priv->on_ping_fn)(priv->user, priv, priv->recv.control.buf, priv->recv.control.used);
        priv->dispatching--;
    } else if (WS_OPCODE_PONG == priv->recv.frame.opcode) {
        priv->dispatching++;
        (priv->on_pong_fn)(priv->user, priv, priv->recv.control.buf, priv->recv.control.used);
        priv->dispatching--;
    } else if (WS_OPCODE_CLOSE == priv->recv.frame.opcode) {
        int status = 1005;
        const char *s = "";
        size_t len = priv->recv.frame.payload_len;

        switch (len) {
            case 0:
                break;
            case 1:
                /* Invalid as 0 or 2+ bytes are needed */
                status = 1002;
                len = 0;
                cws_close(priv, status, "invalid close payload length", SIZE_MAX);
                break;
            default:
                status = priv->recv.control.buf[0] << 8 | priv->recv.control.buf[1];
                if (!_is_valid_close_from_server(status)) {
                    status = 1002;
                    len = 0;
                    cws_close(priv, status, "invalid close reason", SIZE_MAX);
                } else {
                    /* Ensure there is a trailing '\0' to prevent buffer overruns */
                    priv->recv.control.buf[priv->recv.frame.payload_len] = '\0';
                    s = (const char*) &priv->recv.control.buf[2];
                    len -= 2;
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
}

static inline size_t _min_size_t(size_t a, size_t b)
{
    return (a < b) ? a : b;
}
