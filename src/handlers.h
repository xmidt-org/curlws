/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __HANDLERS_H__
#define __HANDLERS_H__

#include <curlws/curlws.h>

#include "internal.h"

/**
 * Helper function that takes the user provided configuration and wires
 * up the combination of the defaults and the user values into the CWS
 * object.
 */
void populate_callbacks(struct callbacks *dest, const struct cws_config *src);

#endif
