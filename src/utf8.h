/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __UTF8_H__
#define __UTF8_H__

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/* The maximum number of bytes needed to make any UTF8 encoded character. */
#define MAX_UTF_BYTES   4

/**
 * Returns the number of total bytes needed by the UTF8 starting character
 * passed in.
 *
 * @retval 0 if the character is invalid
 * @ratval [1, 2, 3, 4] the total number of bytes needed (including this byte)
 */
size_t utf8_get_size(char c);


/**
 * Returns if the starting sequence could possible be valid UTF8 or if it is
 * already out of bounds.
 *
 * @param string the text to validate is UTF8
 * @param len    the length of the string in bytes
 *
 * @return true if the sequence could possibly be valid, false if it is
 *         definitely not possibly valid.
 */
bool utf8_maybe_valid(const char *text, size_t len);


/**
 * Checks a string to ensure only valid UTF-8 character sequences are present
 * and returns the number of bytes left over (of there are any).
 *
 * @param text   the string to validate is UTF-8
 * @param len    the length of the string in bytes (in)
 *               the length of the string in bytes of all completely present
 *               characters (out)
 *
 * @returns 0 if the string is valid assuming more data may come later, or
 *          less than zero indicates an error 
 */
int utf8_validate(const char *text, size_t *len);

#endif

