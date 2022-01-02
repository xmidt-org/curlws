/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __HEADER_H__
#define __HEADER_H__

#include <curl/curl.h>

#include "internal.h"

/**
 * Initializes the curl connection to be able to process headers
 *
 * @param priv the curlws object configure
 */
CURLcode header_init(CWS *priv);

#endif
