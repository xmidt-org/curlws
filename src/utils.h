/*
 * Copyright (C) 2016 Gustavo Sverzut Barbieri
 * Copyright (c) 2021 Comcast Cable Communications Management, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://opensource.org/licenses/MIT
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
