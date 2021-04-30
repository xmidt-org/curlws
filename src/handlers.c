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
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cb.h"
#include "handlers.h"
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
static int _default_on_fragment(void*, CWS*, int, const void*, size_t);
static int _default_on_ping(void*, CWS*, const void*, size_t);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void populate_callbacks(struct callbacks *dest, const struct cws_config *src)
{
    dest->on_fragment_fn = _default_on_fragment;
    dest->on_ping_fn     = _default_on_ping;

    if (NULL == src) {
        return;
    }

    /* Overwrite the default values with the user specified values. */
    if (src->on_connect) {
        dest->on_connect_fn = src->on_connect;
    }
    if (src->on_text) {
        dest->on_text_fn = src->on_text;
    }
    if (src->on_binary) {
        dest->on_binary_fn = src->on_binary;
    }
    if (src->on_fragment) {
        dest->on_fragment_fn = src->on_fragment;
    }
    if (src->on_ping) {
        dest->on_ping_fn = src->on_ping;
    }
    if (src->on_pong) {
        dest->on_pong_fn = src->on_pong;
    }
    if (src->on_close) {
        dest->on_close_fn = src->on_close;
    }
}


/*----------------------------------------------------------------------------*/

static int _default_on_fragment(void *user, CWS *priv, int info, const void *buffer, size_t len)
{
    int one_frame = (CWS_FIRST | CWS_LAST);

    IGNORE_UNUSED(user);

    if (one_frame == (one_frame & info)) {
        if (CWS_BINARY & info) {
            cb_on_binary(priv, buffer, len);
        } else {
            cb_on_text(priv, buffer, len);
        }
    } else {
        size_t prev_len;
        if (CWS_FIRST & info) {
            priv->stream_type = (CWS_BINARY | CWS_TEXT) & info;
            priv->stream_buffer_len = 0;
        }

        prev_len = priv->stream_buffer_len;
        priv->stream_buffer_len += len;

        if (0 < priv->stream_buffer_len) {
            priv->stream_buffer = realloc(priv->stream_buffer, priv->stream_buffer_len);
            memcpy(&((uint8_t*)priv->stream_buffer)[prev_len], buffer, len);
        }

        if (CWS_LAST & info) {
            if (CWS_BINARY & priv->stream_type) {
                cb_on_binary(priv, priv->stream_buffer, priv->stream_buffer_len);
            } else {
                cb_on_text(priv, priv->stream_buffer, priv->stream_buffer_len);
            }

            if (NULL != priv->stream_buffer) {
                free(priv->stream_buffer);
            }
            priv->stream_buffer = NULL;
            priv->stream_buffer_len = 0;
        }
    }

    return 0;
}



static int _default_on_ping(void *user, CWS *priv, const void *buffer, size_t len)
{
    IGNORE_UNUSED(user);

    cws_pong(priv, buffer, len);

    return 0;
}
