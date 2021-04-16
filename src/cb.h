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
#ifndef __CB_H__
#define __CB_H__

#include <stddef.h>

#include "curlws.h"
#include "internal.h"

/**
 * These are helper wrappers for calling the callback functions.
 */
void cb_on_connect(CWS *priv, const char *protos);
void cb_on_text(CWS *priv, const char *text, size_t len);
void cb_on_binary(CWS *priv, const void *buf, size_t len);
void cb_on_stream(CWS *priv, int info, const void *buf, size_t len);
void cb_on_ping(CWS *priv, const void *buf, size_t len);
void cb_on_pong(CWS *priv, const void *buf, size_t len);
void cb_on_close(CWS *priv, int code, const char *text, size_t len);

#endif

