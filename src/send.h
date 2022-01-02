/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
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
