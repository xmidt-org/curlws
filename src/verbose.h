/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __VERBOSE_H__
#define __VERBOSE_H__

#include "internal.h"

/**
 * Wrapper helper function to deal with verbosity and outputing that to the
 * right place.  It's basically printf() with some logic.
 */
void verbose(CWS *priv, const char *format, ...);

/**
 * Print out the close status of the connection.
 */
void verbose_close(CWS *priv);
#endif

