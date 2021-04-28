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

#include "curlws.h"
#include "data_block_sender.h"
#include "frame_senders.h"
#include "internal.h"

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
CWScode data_block_sender(CWS *priv, int options, const void *data, size_t len)
{
    const uint8_t *buffer = data;

    switch (options) {
        case CWS_BINARY:
        case CWS_TEXT:
            break;
        default:
            return CWSE_INVALID_OPTIONS;
    }

    if (priv->close_state) {
        return CWSE_CLOSED_CONNECTION;
    }

    options |= CWS_FIRST;
    if (!buffer || !len) {
        options |= CWS_LAST;
        return frame_sender_data(priv, options, NULL, 0);
    }

    while (priv->cfg.max_payload_size < len) {
        CWScode rv;

        rv = frame_sender_data(priv, options, buffer, priv->cfg.max_payload_size);
        if (CWSE_OK != rv) {    /* Should only fail if we ran out of memory */
            return rv;
        }

        options = CWS_CONT;
        len -= priv->cfg.max_payload_size;
        buffer += priv->cfg.max_payload_size;
    }

    options |= CWS_LAST;
    return frame_sender_data(priv, options, buffer, len);
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */

