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
#ifndef __SENDING_H__
#define __SENDING_H__

#include <stddef.h>
#include <stdint.h>

#include <curl/curl.h>

#include "frame.h"
#include "internal.h"


/**
 * Initializes the curl connection to be able to send data.
 *
 * @param priv the curlws object to configure
 */
CURLcode send_init(CWS *priv);


/**
 * Cleans up the buffers that are presently in use and discards the data.
 *
 * @param priv the curlws object to operate on
 */
void send_destroy(CWS *priv);

/**
 * Return the buffer size needed for a specified payload size.
 */
size_t send_get_memory_needed(size_t payload_size);


/**
 * Used to send exactly 1 frame of data.
 *
 * @note Sending always requires a mask.
 *
 * @param priv the curlws object to sent data through
 * @param f    the frame to send
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 */
CWScode send_frame(CWS *priv, const struct cws_frame *f);

#endif

