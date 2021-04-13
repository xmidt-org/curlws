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
#include <stddef.h>
#include <stdint.h>

#include "frame_senders.h"
#include "internal.h"
#include "random.h"
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
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
CWScode frame_sender_control(CWS *priv, int options, const void *data, size_t len)
{
    struct cws_frame f = {
        .fin = 1,
        .mask = 1,
        .is_control = 1,
        .is_urgent = (CWS_URGENT & options) ? 1 : 0,
    };

    /* No special labels are allowed for a control message */
    if (options & ~CWS_CTRL_MASK) {
        return CWSE_INVALID_OPTIONS;
    }

    switch (options & CWS_CTRL_MASK) {
        case CWS_CLOSE:
            f.opcode = WS_OPCODE_CLOSE;
            break;
        case CWS_PING:
            f.opcode = WS_OPCODE_PING;
            break;
        case CWS_PONG:
            f.opcode = WS_OPCODE_PONG;
            break;
        default:
            return CWSE_INVALID_OPTIONS;
    }

    if (priv->closed) {
        return CWSE_CLOSED_CONNECTION;
    }

    if (!data || !len) {
        data = NULL;
        len = 0;
    }

    f.payload = data;
    f.payload_len = len;

    if (WS_CTL_PAYLOAD_MAX < len) {
        return CWSE_APP_DATA_LENGTH_TOO_LONG;
    }

    cws_random(priv, f.masking_key, 4);

    return send_frame(priv, &f);
}


CWScode frame_sender_data(CWS *priv, int options, const void *data, size_t len)
{
    struct cws_frame f = {
        .fin = (CWS_LAST & options) ? 1 : 0,
        .mask = 1,
        .is_control = 0,
    };
    const int allowed = (CWS_NONCTRL_MASK | CWS_FIRST | CWS_LAST);
    int lastinfo = priv->last_sent_data_frame_info;

    if (options != (options & allowed)) {
        return CWSE_INVALID_OPTIONS;
    }

    switch (options & (CWS_CONT|CWS_BINARY|CWS_TEXT)) {
        case CWS_CONT:
            if (CWS_FIRST & options) {
                return CWSE_INVALID_OPTIONS;
            }
            if ((0 == lastinfo) || (CWS_LAST & lastinfo)) {
                return CWSE_STREAM_CONTINUITY_ISSUE;
            }
            f.opcode = WS_OPCODE_CONTINUATION;
            break;

        case CWS_BINARY:
        case CWS_TEXT:
            if (!(CWS_FIRST & options)) {
                return CWSE_INVALID_OPTIONS;
            }
            if ((0 != lastinfo) && !(CWS_LAST & lastinfo)) {
                return CWSE_STREAM_CONTINUITY_ISSUE;
            }

            f.opcode = WS_OPCODE_BINARY;
            if (options & CWS_TEXT) {
                f.opcode = WS_OPCODE_TEXT;
            }
            break;

        default:
            return CWSE_INVALID_OPTIONS;
    }

    if (priv->closed) {
        return CWSE_CLOSED_CONNECTION;
    }

    priv->last_sent_data_frame_info = options;

    if (!data || !len) {
        data = NULL;
        len = 0;
    }

    f.payload = data;
    f.payload_len = len;

    cws_random(priv, f.masking_key, 4);

    return send_frame(priv, &f);
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */

