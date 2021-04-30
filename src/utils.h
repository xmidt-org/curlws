/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Encodes a buffer of a specified length as a base64 encoded string.
 *
 * @note: The output buffer provided MUST be at least large enough to contain
 *        the encoded string and a trailing '\0' or a buffer overflow will
 *        occur.
 *
 * @param in    the input buffer
 * @param len   the input buffer length in bytes
 * @param out   the output buffer (MUST be large enough to encode into)
 */
void cws_encode_base64(const void *in, const size_t len, char *out);


/**
 * Trims the whitespace from the beginning and end of a string.  Returns a
 * pointer to the start of the first non-whitespace character and a length to
 * the last non-whitespace character.
 *
 * @note: Do not free the pointer returned!  It is from inside the specified
 *        buffer and may do undefined things depending on your libc.
 *
 * @param s     the input string
 * @param len   the length of the input buffer
 *
 * @return the pointer to the start of the non-whitespace (or NULL)
 */
const char* cws_trim(const char *s, size_t *len);


/**
 * Checks to see if the 'prefix' string is at the start of the buffer.
 *
 * @note: This is a case insensitive match.
 *
 * @param s      the input string
 * @param len    the length of the input string to process
 * @param prefix the string to look for at the beginning of the buffer
 *
 * @return true if found, false otherwise
 */
bool cws_has_prefix(const char *s, const size_t len, const char *prefix);


/**
 * Changes the 'ws://' and 'wss://' to 'http://' and 'https://' respectively in
 * the url.
 *
 * @note: This function ALWAYS returns a new string that must be free()d
 *
 * @note: Curl doesn't support 'ws://' or 'wss://' scheme.
 *
 * @param url the url to rewrite
 *
 * @return the rewritten string.
 */
char* cws_rewrite_url(const char *url);


/**
 * strdup() is a non-standard libc function.  To be most portable, this library
 * provides it's own implementation for this simple function.
 */
char* cws_strdup(const char *s);


/**
 * strndup() is a non-standard libc function.  To be most portable, this library
 * provides it's own implementation for this simple function.
 */
char* cws_strndup(const char *s, size_t n);


/**
 * strncasecmp() is a non-standard libc function.  To be most portable, this
 * library provides it's own implementation for this simple function.
 */
int cws_strncasecmp(const char *s1, const char *s2, size_t n);


/**
 * cws_strmerge() takes two strings and safely contatinates them into
 * one that is returned.
 *
 * @note The returned buffer must be free()ed.
 *
 * @return string = s1 + s2
 */
char* cws_strmerge(const char *s1, const char *s2);


#endif
