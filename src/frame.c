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
#include "frame.h"

#include <stddef.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int frame_validate(const struct cws_frame *f, frame_dir dir)
{
    if (FRAME_DIR_C2S == dir) {
        if ((1 != f->mask) || (0 == f->masking_key)) {
            return -1;
        }
    } else {
        if ((0 != f->mask) || (0 != f->masking_key)) {
            return -1;
        }
    }

    switch (f->opcode) {
        case WS_OPCODE_CONTINUATION:
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY:
            if (0 != f->is_control) {
                return -2;
            }
            if (UINT64_MAX == f->payload_len) {
                return -3;
            }
            break;

        case WS_OPCODE_CLOSE:
        case WS_OPCODE_PING:
        case WS_OPCODE_PONG:
            if (1 != f->is_control) {
                return -2;
            }
            if (1 != f->fin) {
                return -4;
            }
            if (WS_CTL_PAYLOAD_MAX < f->payload_len) {
                return -3;
            }
            break;
        default:
            return -5;
    }

    return 0;
}

int frame_decode(struct cws_frame *in, const void *buffer, size_t len, ssize_t *delta)
{
    struct cws_frame f;
    const uint8_t *buf = (const uint8_t*) buffer;

    if (delta) {
        *delta = 0;
    }

    memset(&f, 0, sizeof(struct cws_frame));
    memset(in, 0, sizeof(struct cws_frame));

    if (len < 2) {
        if (delta) {
            *delta = len - 2;
        }
        return 0;
    }

    f.fin = 0;
    if (0x80 & (*buf)) {
        f.fin = 1;
    }

    if (0x70 & (*buf)) {
        return -1;
    }

    f.opcode = 0x0f & (*buf);
    switch (f.opcode) {
        case WS_OPCODE_CONTINUATION:
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY:
            f.is_control = 0;
            break;
        case WS_OPCODE_CLOSE:
        case WS_OPCODE_PING:
        case WS_OPCODE_PONG:
            f.is_control = 1;
            break;
        default:
            return -2;
    }
        
    buf++;
    len--;

    f.mask = 0;
    if (0x80 & (*buf)) {
        f.mask = 1;
    }

    f.payload_len = 0x7f & (*buf);

    buf++;
    len--;

    /* Deal with possible longer lengths */
    if (126 == f.payload_len) {
        if (len < 2) {
            if (delta) {
                *delta = len - 2;
            }
            return 0;
        }
        f.payload_len = (0xff & buf[0]) << 8 | (0xff & buf[1]);
        buf += 2;
        len -= 2;

        /* Length must be in the smallest size possible. */
        if (f.payload_len < 126) {
            return -3;
        }
    } else if (127 == f.payload_len) {
        if (len < 8) {
            if (delta) {
                *delta = len - 8;
            }
            return 0;
        }
        if (0x80 & buf[0]) {
            return -4;
        }
        f.payload_len = ((uint64_t) (0x7f & buf[0])) << 56 |
                        ((uint64_t) (0xff & buf[1])) << 48 |
                        ((uint64_t) (0xff & buf[2])) << 40 |
                        ((uint64_t) (0xff & buf[3])) << 32 |
                        ((uint64_t) (0xff & buf[4])) << 24 |
                        ((uint64_t) (0xff & buf[5])) << 16 |
                        ((uint64_t) (0xff & buf[6])) <<  8 |
                        ((uint64_t) (0xff & buf[7]));
        buf += 8;
        len -= 8;
        if (f.payload_len <= UINT16_MAX) {
            return -5;
        }
    }

    f.masking_key = 0;
    if (f.mask) {
        if (len < 4) {
            if (delta) {
                *delta = len - 4;
            }
            return 0;
        }
        f.masking_key = (0xff & buf[0]) << 24 |
                        (0xff & buf[1]) << 16 |
                        (0xff & buf[2]) <<  8 |
                        (0xff & buf[3]);
        buf += 4;
        len -= 4;
    }

    if (delta) {
        *delta = ((const void*) buf) - buffer;
    }

    /* Only when we're 100% complete and correct will we copy over the values */
    memcpy(in, &f, sizeof(struct cws_frame));
    return 0;
}


size_t frame_encode(const struct cws_frame *f, void *buf, size_t len)
{
    size_t header_len;
    uint8_t *mask, *p, *payload;
    size_t i;

    /* Do a simple check to make sure we don't overwrite the buffer.  This
     * doesn't need to be super accurate since we only need to account for
     * the potential difference between a 6 byte and 14 byte header. */
    if (len < (WS_FRAME_HEADER_MIN + WS_FRAME_HEADER_MASK) + f->payload_len) {
        return 0;
    }

    p = (uint8_t*) buf;

    p[0] = (f->fin ? 0x80 : 0) | (0x0f & f->opcode);
    if (f->payload_len <= 125) {
        p[1] = 0x80 | (uint8_t) (0x7f & f->payload_len);
        mask = &p[2];
        header_len = 6;
    } else if (f->payload_len <= UINT16_MAX) {
        p[1] = 0x80 | 126;
        p[2] = (uint8_t) (0x00ff & (f->payload_len >> 8));
        p[3] = (uint8_t) (0x00ff & f->payload_len);
        mask = &p[4];
        header_len = 8;
    } else {
        p[1] = 0x80 | 127;
        memset(&p[2], 0, 6);    /* In case sizeof(size_t) is less than 8 bytes
                                 * zero the upper bits. */
        if (8 == sizeof(size_t)) {
            p[2] = (uint8_t) (0x00ff & (f->payload_len >> 56));
            p[3] = (uint8_t) (0x00ff & (f->payload_len >> 48));
            p[4] = (uint8_t) (0x00ff & (f->payload_len >> 40));
            p[5] = (uint8_t) (0x00ff & (f->payload_len >> 32));
        }

        if (2 < sizeof(size_t)) {
            p[6] = (uint8_t) (0x00ff & (f->payload_len >> 24));
            p[7] = (uint8_t) (0x00ff & (f->payload_len >> 16));
        }

        p[8] = (uint8_t) (0x00ff & (f->payload_len >>  8));
        p[9] = (uint8_t) (0x00ff & f->payload_len);
        mask = &p[10];
        header_len = 14;
    }

    /* Now that we know how large the header really is, check again. */
    if (len < header_len + f->payload_len) {
        return 0;
    }

    mask[0] = 0x00ff & (f->masking_key >> 24);
    mask[1] = 0x00ff & (f->masking_key >> 16);
    mask[2] = 0x00ff & (f->masking_key >> 8);
    mask[3] = 0x00ff & (f->masking_key);
    
    p += header_len;
    payload = (uint8_t*) f->payload;

    /* Mask the payload and write it to the buf */
    for (i = 0; i < f->payload_len; i++) {
        p[i] = payload[i] ^ mask[0x3 & i];
    }
    return header_len + f->payload_len;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */
