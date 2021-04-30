/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
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
void cb_on_fragment(CWS *priv, int info, const void *buf, size_t len);
void cb_on_ping(CWS *priv, const void *buf, size_t len);
void cb_on_pong(CWS *priv, const void *buf, size_t len);
void cb_on_close(CWS *priv, int code, const char *text, size_t len);

#endif

