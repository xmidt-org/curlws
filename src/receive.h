/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __RECEIVE_H__
#define __RECEIVE_H__

#include <curl/curl.h>

#include "internal.h"

/**
 * Initializes the curl connection to be able to receive data.
 *
 * @param priv the curlws object configure
 */
CURLcode receive_init(CWS *priv);

#endif

