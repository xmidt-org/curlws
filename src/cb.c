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
#include "curlws.h"
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
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void __process_rv(CWS*, int);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void cb_on_connect(CWS *priv, const char *protos)
{
    verbose(priv, "< websocket on_connect() protos: '%s'\n", protos);

    if (priv->cb.on_connect_fn) {
        int rv;

        rv = (*priv->cb.on_connect_fn)(priv->cfg.user, priv, protos);
        __process_rv(priv, rv);
    }

    verbose(priv, "> websocket on_connect()\n");
}


void cb_on_text(CWS *priv, const char *text, size_t len)
{
    if (priv->cfg.verbose) {
        int truncated;
        char *dots = "...";

        truncated = 40;
        if (len <= (size_t) truncated) {
            truncated = (int) len;
            dots = "";
        }
        verbose(priv, "< websocket on_text() len: %zd, text: '%.*s%s'\n", len,
                truncated, text, dots);
    }

    if (priv->cb.on_text_fn) {
        int rv;

        rv = (*priv->cb.on_text_fn)(priv->cfg.user, priv, text, len);
        __process_rv(priv, rv);
    }

    verbose(priv, "> websocket on_text()\n");
}


void cb_on_binary(CWS *priv, const void *buf, size_t len)
{
    verbose(priv, "< websocket on_binary() len: %zd, [buf]\n", len);

    if (priv->cb.on_binary_fn) {
        int rv;

        rv = (*priv->cb.on_binary_fn)(priv->cfg.user, priv, buf, len);
        __process_rv(priv, rv);
    }

    verbose(priv, "> websocket on_binary()\n");
}


void cb_on_fragment(CWS *priv, int info, const void *buf, size_t len)
{
    int rv;

    verbose(priv, "< websocket on_fragment() info: 0x%08x, len: %zd, [buf]\n", info, len);

    /* Always present since there is a default handler & clients cannot
     * overwrite using the value NULL as that indicates use the default. */
    rv = (*priv->cb.on_fragment_fn)(priv->cfg.user, priv, info, buf, len);
    __process_rv(priv, rv);

    verbose(priv, "> websocket on_fragment()\n");
}


void cb_on_ping(CWS *priv, const void *buf, size_t len)
{
    int rv;

    verbose(priv, "< websocket on_ping() len: %zd, [buf]\n", len);

    /* Always present since there is a default handler & clients cannot
     * overwrite using the value NULL as that indicates use the default. */
    rv = (*priv->cb.on_ping_fn)(priv->cfg.user, priv, buf, len);
    __process_rv(priv, rv);

    verbose(priv, "> websocket on_ping()\n");
}


void cb_on_pong(CWS *priv, const void *buf, size_t len)
{
    verbose(priv, "< websocket on_pong() len: %zd, [buf]\n", len);

    if (priv->cb.on_pong_fn) {
        int rv;

        rv = (*priv->cb.on_pong_fn)(priv->cfg.user, priv, buf, len);
        __process_rv(priv, rv);
    }

    verbose(priv, "> websocket on_pong()\n");
}


void cb_on_close(CWS *priv, int code, const char *text, size_t len)
{
    if (text) {
        if (SIZE_MAX == len) {
            len = strlen(text);
        }
    } else {
        len = 0;
    }

    verbose(priv, "< websocket on_close() code: %d, len: %zd, text: '%.*s'\n",
            code, len, (int) len, text);

    if (priv->cb.on_close_fn) {
        int rv;

        rv = (*priv->cb.on_close_fn)(priv->cfg.user, priv, code, text, len);
        __process_rv(priv, rv);
    }

    verbose(priv, "> websocket on_close()\n");
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void __process_rv(CWS *priv, int code)
{
    if (!code) {
        return;
    }

    if (false == is_close_code_valid(code)) {
        code = 1011;
    }
    cws_close(priv, code, NULL, 0);
}

