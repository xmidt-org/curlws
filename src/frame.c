/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include "frame.h"

#include <stdbool.h>
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
static int _frame_decode(struct cws_frame *, const void *, size_t, long *);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int frame_validate(const struct cws_frame *f, frame_dir dir)
{
    if (((FRAME_DIR_C2S == dir) && (1 != f->mask)) || ((FRAME_DIR_S2C == dir) && (0 != f->mask)))
    {
        return -1;
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


int frame_decode(struct cws_frame *in, const void *buffer, size_t len, long *delta)
{
    if (NULL == delta) {
        long tmp;
        return _frame_decode(in, buffer, len, &tmp);
    }
    return _frame_decode(in, buffer, len, delta);
}


size_t frame_encode(const struct cws_frame *f, void *buf, size_t len)
{
    size_t header_len;
    uint8_t *mask          = NULL;
    uint8_t *p             = NULL;
    const uint8_t *payload = NULL;

    /* Do a simple check to make sure we don't overwrite the buffer.  This
     * doesn't need to be super accurate since we only need to account for
     * the potential difference between a 6 byte and 14 byte header. */
    if (len < (WS_FRAME_HEADER_MIN + WS_FRAME_HEADER_MASK) + f->payload_len) {
        return 0;
    }

    p = (uint8_t *) buf;

    p[0] = (f->fin ? 0x80 : 0) | (0x0f & f->opcode);
    if (f->payload_len <= 125) {
        p[1]       = 0x80 | (uint8_t) (0x7f & f->payload_len);
        mask       = &p[2];
        header_len = 6;
    } else if (f->payload_len <= UINT16_MAX) {
        p[1]       = 0x80 | 126;
        p[2]       = (uint8_t) (0x00ff & (f->payload_len >> 8));
        p[3]       = (uint8_t) (0x00ff & f->payload_len);
        mask       = &p[4];
        header_len = 8;
    } else {
        p[1] = 0x80 | 127;
        memset(&p[2], 0, 6); /* In case sizeof(size_t) is less than 8 bytes
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

        p[8]       = (uint8_t) (0x00ff & (f->payload_len >> 8));
        p[9]       = (uint8_t) (0x00ff & f->payload_len);
        mask       = &p[10];
        header_len = 14;
    }

    /* Now that we know how large the header really is, check again. */
    if (len < header_len + f->payload_len) {
        return 0;
    }

    memcpy(mask, f->masking_key, 4);

    p += header_len;
    payload = (const uint8_t *) f->payload;

    /* Mask the payload and write it to the buf */
    for (size_t i = 0; i < f->payload_len; i++) {
        p[i] = payload[i] ^ mask[0x3 & i];
    }
    return header_len + f->payload_len;
}


const char *frame_opcode_to_string(const struct cws_frame *f)
{
    static const char *map[] = {
        "CONT",          /* 0x0 */
        "TEXT",          /* 0x1 */
        "BINARY",        /* 0x2 */
        "Unknown (0x3)", /* 0x3 */
        "Unknown (0x4)", /* 0x4 */
        "Unknown (0x5)", /* 0x5 */
        "Unknown (0x6)", /* 0x6 */
        "Unknown (0x7)", /* 0x7 */
        "CLOSE",         /* 0x8 */
        "PING",          /* 0x9 */
        "PONG",          /* 0xa */
        "Unknown (0xb)", /* 0xb */
        "Unknown (0xc)", /* 0xc */
        "Unknown (0xd)", /* 0xd */
        "Unknown (0xe)", /* 0xe */
        "Unknown (0xf)", /* 0xf */
    };

    if (f) {
        return map[f->opcode];
    }
    return "invalid frame";
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static int _frame_decode(struct cws_frame *in, const void *buffer, size_t len, long *delta)
{
    struct cws_frame f;
    const uint8_t *buf = (const uint8_t *) buffer;

    memset(&f, 0, sizeof(struct cws_frame));
    memset(in, 0, sizeof(struct cws_frame));

    *delta = 0;

    if (len < 2) {
        *delta = len - 2;
        return 0;
    }

    f.fin = (0x80 & (*buf)) ? 1 : 0;

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
    f.mask = (0x80 & (*buf)) ? 1 : 0;

    f.payload_len = 0x7f & (*buf);

    buf++;
    len--;

    /* Deal with possible longer lengths */
    if (126 == f.payload_len) {
        if (len < 2) {
            *delta = len - 2;
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
            *delta = len - 8;
            return 0;
        }
        if (0x80 & buf[0]) {
            return -4;
        }
        f.payload_len = ((uint64_t) (0x7f & buf[0])) << 56 | ((uint64_t) (0xff & buf[1])) << 48 | ((uint64_t) (0xff & buf[2])) << 40 | ((uint64_t) (0xff & buf[3])) << 32 | ((uint64_t) (0xff & buf[4])) << 24 | ((uint64_t) (0xff & buf[5])) << 16 | ((uint64_t) (0xff & buf[6])) << 8 | ((uint64_t) (0xff & buf[7]));
        buf += 8;
        len -= 8;
        if (f.payload_len <= UINT16_MAX) {
            return -5;
        }
    }

    memset(f.masking_key, 0, 4);
    if (f.mask) {
        if (len < 4) {
            *delta = len - 4;
            return 0;
        }
        memcpy(f.masking_key, buf, 4);
        buf += 4;
    }

    *delta = buf - ((const uint8_t *) buffer);

    /* Only when we're 100% complete and correct will we copy over the values */
    memcpy(in, &f, sizeof(struct cws_frame));
    return 0;
}
