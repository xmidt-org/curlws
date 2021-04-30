/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __RANDOM_H__
#define __RANDOM_H__

#include <stddef.h>

#include "internal.h"

/**
 * Fill the 'buffer' of length 'len' bytes with random data & return.
 *
 * @param priv   the websocket object associated with the request
 * @param buffer the buffer to fill
 * @param len    the length of the buffer
 */
void cws_random(CWS *priv, void *buffer, size_t len);

#endif

