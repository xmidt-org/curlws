/*
 * Copyright (C) 2016 Gustavo Sverzut Barbieri
 * Copyright (c) 2021 Comcast Cable Communications Management, LLC
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * https://opensource.org/licenses/MIT
 */
#include <stdint.h>
#include <string.h>

#include "utf8.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/

/* General notes:
 *
 * Anything that falls outside the ranges specified is invalid.
 * This site seems to be the simplest to understand about the topic:
 *
 *  https://linux.die.net/man/7/utf8
 *
 *
 * 0x00000000 - 0x0000007F: (basically ASCII)
 *     0xxxxxxx
 * 0x00000080 - 0x000007FF:
 *     110xxxxx 10xxxxxx
 * 0x00000800 - 0x0000FFFF:
 *     1110xxxx 10xxxxxx 10xxxxxx
 * 0x00010000 - 0x001FFFFF:
 *     11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 * Exclude the block 0xd800 - 0xdfff
 */


/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/

/* This fun structure is a lookup map for the 1st byte to figure out how
 * many bytes are needed to represent the character.  Any lenght that is
 * invalid is 0 so it's easy to process.
 */
static const uint8_t utf8_len[256] = {
     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
      *---------------------------------------------------------------*/
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x00-0x0f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x10-0x1f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x20-0x2f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x30-0x3f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x40-0x4f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x50-0x5f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x60-0x6f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x70-0x7f */

     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f                */

        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf */
        0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  /* 0xc0-0xcf */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  /* 0xd0-0xdf */
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /* 0xe0-0xef */
        4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   /* 0xf0-0xff */
    };

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
size_t utf8_get_size(char c)
{
    return (size_t) utf8_len[(uint8_t)c];
}


bool utf8_maybe_valid(const char *text, size_t len)
{
    uint8_t buf[4] = { 0, 0x80, 0x80, 0x80 };
    ssize_t c_len;

    memcpy(buf, text, len);
    c_len = utf8_len[*buf];

    if (0 == c_len) {
        return false;
    }

    if (c_len == utf8_validate((char*)buf, c_len)) {
        return true;
    }

    return false;
}


ssize_t utf8_validate(const char *text, size_t len)
{
    const uint8_t *bytes = (const uint8_t*) text;
    size_t left = len;

    while (left) {
        size_t c_len = utf8_len[*bytes];

        if (left < c_len) {
            return len - left;
        }

        if (0 == c_len) {
            return -1;
        } else if (1 == c_len) {
            /* All is fine. */
        } else if (2 == c_len) {
            if (0x80 != (0xc0 & bytes[1])) {
                return -1;
            }
        } else if (3 == c_len) {
            int c = (0x0f & bytes[0]) << 12 |
                    (0x3f & bytes[1]) << 6 |
                    (0x3f & bytes[2]);

            if ((c < 0x800) ||
                ((0xd800 <= c) && (c <= 0xdfff)) ||
                (0x80 != (0xc0 & bytes[1])) ||
                (0x80 != (0xc0 & bytes[2])))
            {
                return -1;
            }
        } else {
            int c = (0x07 & bytes[0]) << 18 |
                    (0x3f & bytes[1]) << 12 |
                    (0x3f & bytes[2]) << 6  |
                    (0x3f & bytes[3]);

            if ((c < 0x10000) || (0x10ffff < c) ||
                (0x80 != (0xc0 & bytes[1])) ||
                (0x80 != (0xc0 & bytes[2])) ||
                (0x80 != (0xc0 & bytes[3])))
            {
                return -1;
            }
        }

        left -= c_len;
        bytes += c_len;
    }

    return len - left;
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */

