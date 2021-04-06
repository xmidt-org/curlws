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
#ifndef __DATA_BLOCK_SENDER_H__
#define __DATA_BLOCK_SENDER_H__

#include <stddef.h>

/**
 * Used to send an entire blob of data regardless of the size.  This function
 * will split up the blob into more manageable sized chucks and send them.
 *
 * @param priv    the curlws object to sent data through
 * @param options only one of the following: CWS_TEXT or CWS_BINARY
 * @param data    the payload data to send (may be NULL)
 * @param len     the number of bytes in the payload (may be 0)
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_INTERNAL_ERROR
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_INVALID_OPTIONS
 */
CWScode data_block_sender(CWS *priv, int options, const void *data, size_t len);

#endif

