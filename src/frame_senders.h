/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __FRAME_SENDERS_H__
#define __FRAME_SENDERS_H__

#include <stddef.h>
#include <stdint.h>

#include "curlws.h"

#define CWS_CLOSE           0x00010000
#define CWS_PING            0x00020000
#define CWS_PONG            0x00040000
#define CWS_CTRL_MASK       (CWS_CLOSE|CWS_PING|CWS_PONG)
#define CWS_NONCTRL_MASK    (CWS_CONT|CWS_BINARY|CWS_TEXT)
#define CWS_URGENT          0x04000000

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
 *                CWS_CONT | CWS_LAST
 *                CWS_BINARY | CWS_FIRST
 *                CWS_BINARY | CWS_FIRST | CWS_LAST
 *                CWS_TEXT   | CWS_FIRST
 *                CWS_TEXT   | CWS_FIRST | CWS_LAST
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

