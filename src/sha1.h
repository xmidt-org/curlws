/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __SHA1_H__
#define __SHA1_H__

#include <stddef.h>

/**
 * Calculates the SHA-1 value for a buffer of a specified length.
 * Expects an output buffer of at least 20 bytes or a buffer overflow will
 * occur.
 *
 * @param in    the buffer to SHA-1 over
 * @param len   the length of the in buffer in bytes
 * @param out   the pointer to the preallocated buffer that holds the output
 */
void cws_sha1(const void *in, size_t len, void *out);
#endif

