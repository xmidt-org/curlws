/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
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
