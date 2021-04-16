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
#include <string.h>

#include "cb.h"

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
void cb_on_connect(CWS *priv, const char *protos)
{
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_connect protos: '%s'\n", protos);
    }

    (*priv->cb.on_connect_fn)(priv->cfg.user, priv, protos);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_connect\n");
    }
}


void cb_on_text(CWS *priv, const char *text, size_t len)
{
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_text len: %zd, text: '%.*s'\n", len, (int) len, text);
    }

    (*priv->cb.on_text_fn)(priv->cfg.user, priv, text, len);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_text\n");
    }
}


void cb_on_binary(CWS *priv, const void *buf, size_t len)
{
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_binary len: %zd, [buf]\n", len);
    }

    (*priv->cb.on_binary_fn)(priv->cfg.user, priv, buf, len);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_binary\n");
    }
}


void cb_on_stream(CWS *priv, int info, const void *buf, size_t len)
{
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_stream info: 0x%08x, len: %zd, [buf]\n", info, len);
    }

    (*priv->cb.on_stream_fn)(priv->cfg.user, priv, info, buf, len);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_stream\n");
    }
}


void cb_on_ping(CWS *priv, const void *buf, size_t len)
{
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_ping len: %zd, [buf]\n", len);
    }

    (*priv->cb.on_ping_fn)(priv->cfg.user, priv, buf, len);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_ping\n");
    }
}


void cb_on_pong(CWS *priv, const void *buf, size_t len)
{
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_pong len: %zd, [buf]\n", len);
    }

    (*priv->cb.on_pong_fn)(priv->cfg.user, priv, buf, len);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_pong\n");
    }
}


void cb_on_close(CWS *priv, int code, const char *text, size_t len)
{
    if (SIZE_MAX == len) {
        len = strlen(text);
    }
    if (priv->cfg.verbose) {
        fprintf(stderr, "< websocket on_close code: %d, len: %zd, text: '%.*s'\n",
                code, len, (int) len, text);
    }


    (*priv->cb.on_close_fn)(priv->cfg.user, priv, code, text, len);

    if (priv->cfg.verbose) {
        fprintf(stderr, "> websocket on_close\n");
    }
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */

