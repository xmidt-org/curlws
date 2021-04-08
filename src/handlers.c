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
static void _default_on_connect(void*, CWS*, const char*);
static void _default_on_text(void*, CWS*, const char*, size_t);
static void _default_on_binary(void*, CWS*, const void*, size_t);
static void _default_on_stream(void*, CWS*, int, const void*, size_t);
static void _default_on_ping(void*, CWS*, const void*, size_t);
static void _default_on_pong(void*, CWS*, const void*, size_t);
static void _default_on_close(void*, CWS*, int, const char*, size_t);
static void _default_configure(void*, CWS*, CURL*);
static void _default_debug(void*, CWS*, const char*, ...);
static void _default_verbose_debug(void*, CWS*, const char*, ...);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void populate_callbacks(CWS *dest, const struct cws_config *src)
{
    dest->on_connect_fn = _default_on_connect;
    dest->on_text_fn    = _default_on_text;
    dest->on_binary_fn  = _default_on_binary;
    dest->on_stream_fn  = _default_on_stream;
    dest->on_ping_fn    = _default_on_ping;
    dest->on_pong_fn    = _default_on_pong;
    dest->on_close_fn   = _default_on_close;
    dest->debug_fn      = _default_debug;
    dest->configure_fn  = _default_configure;

    if (NULL == src) {
        return;
    }

    /* Optionally select the verbose debug handler */
    if (0 != (1 & src->verbose)) {
        dest->debug_fn = _default_verbose_debug;
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
    if (src->on_stream) {
        dest->on_stream_fn = src->on_stream;
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
    if (src->configure) {
        dest->configure_fn = src->configure;
    }
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void _default_on_connect(void *data, CWS *priv, const char *websocket_protocols)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(websocket_protocols);
}


static void _default_on_text(void *data, CWS *priv, const char *text, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(text);
    IGNORE_UNUSED(len);
}


static void _default_on_binary(void *data, CWS *priv, const void *mem, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(mem);
    IGNORE_UNUSED(len);
}


static void _default_on_stream(void *data, CWS *priv, int info, const void *mem, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(info);
    IGNORE_UNUSED(mem);
    IGNORE_UNUSED(len);
}



static void _default_on_ping(void *data, CWS *priv, const void *mem, size_t len)
{
    IGNORE_UNUSED(data);

    cws_pong(priv, mem, len);
}


static void _default_on_pong(void *data, CWS *priv, const void *mem, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(mem);
    IGNORE_UNUSED(len);
}


static void _default_on_close(void *data, CWS *priv, int status, const char *reason, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(status);
    IGNORE_UNUSED(reason);
    IGNORE_UNUSED(len);
}


static void _default_configure(void *data, CWS *priv, CURL *easy)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(easy);
}


static void _default_debug(void *data, CWS *priv, const char *format, ...)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(format);
}


static void _default_verbose_debug(void *data, CWS *priv, const char *format, ...)
{
    va_list args;

    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
