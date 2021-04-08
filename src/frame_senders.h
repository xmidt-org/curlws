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
#ifndef __FRAME_SENDERS_H__
#define __FRAME_SENDERS_H__

#include <stddef.h>
#include <stdint.h>

#include "curlws.h"

#define CWS_CLOSE           0x001000
#define CWS_CLOSE_URGENT    0x002000
#define CWS_PING            0x004000
#define CWS_PONG            0x008000

/**
 * Used to send a control frame.
 *
 * @param priv    the curlws object to sent data through
 * @param options only one of the following: CWS_CLOSE or CWS_PING or CWS_PONG
 * @param data    the optional payload data to send
 * @param len     the number of bytes in the payload
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_INTERNAL_ERROR
 * @retval CWSE_APP_DATA_LENGTH_TOO_LONG
 * @retval CWSE_CLOSED_CONNECTION
 */
CWScode frame_sender_control(CWS *priv, int options, const void *data, size_t len);


/**
 * Used to send a single data frame.
 *
 * @note This call depends on the previously queued frame state as well.  If
 *       a stream continuity issue is detected, the frame is rejected.
 *
 * @param priv    the curlws object to sent data through
 * @param options only one of the following rows is valid:
 *                CWS_CONT
 *                CWS_CONT | CWS_LAST_FRAME
 *                CWS_BINARY | CWS_FIRST_FRAME
 *                CWS_BINARY | CWS_FIRST_FRAME | CWS_LAST_FRAME
 *                CWS_TEXT   | CWS_FIRST_FRAME
 *                CWS_TEXT   | CWS_FIRST_FRAME | CWS_LAST_FRAME
 *
 * @param data   the optional payload data to send
 * @param len    the number of bytes in the payload
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_INTERNAL_ERROR
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_STREAM_CONTINUITY_ISSUE
 */
CWScode frame_sender_data(CWS *priv, int options, const void *data, size_t len);

#endif

