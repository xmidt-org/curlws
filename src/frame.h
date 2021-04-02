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
#ifndef __FRAME_H__
#define __FRAME_H__

#include <stddef.h>

#include "curlws.h"
#include "ws.h"


typedef enum {
    FRAME_DIR_C2S,  /* Client to server */
    FRAME_DIR_S2C   /* Server to client */
} frame_dir;


struct cws_frame {
    uint8_t fin        : 1; /* 0/1 FIN bit from rfc6455, page 28 */
    uint8_t mask       : 1; /* 0/1 Mask bit from rfc6455, page 29 */
    uint8_t is_control : 1; /* 1 if the opcode is control, 0 otherwise */
    uint8_t opcode     : 4; /* 0-15 opcode from rfc6455, page 29 */

    uint32_t masking_key;   /* The 32 bit masking key to use on upstream msgs */

    uint64_t payload_len;   /* The payload length pointed to by payload */
    const void *payload;    /* The payload (may be NULL) */
};


/**
 * Validates that a frame is reasonable.
 *
 * @note What is not validated:
 *      The payload pointer.
 *
 * @note Only the first detected error is returned.
 *
 * @param f     the frame to validate
 * @param dir   the direction the frame is traveling (Client <-?-> Server)
 *
 * @retval  0 if successful, error otherwise
 * @retval -1 if the mask bit or masking_key are incorrect
 * @retval -2 if the control bit is incorrect
 * @retval -3 if the payload length is invalid
 * @retval -4 if the fin bit is not set for a control frame
 * @retval -5 if the opcode is not known/understood
 */
int frame_validate(const struct cws_frame *f, frame_dir dir);

/**
 * Processes a byte stream into a frame structure in a portable way.
 *
 * @note Always sets delta to the size needed in the event that the frame,
 *       or buffer are NULL.
 *
 * @param f     the frame pointer to fill in
 * @param buf   the buffer to process
 * @param len   the number of bytes that are valid in the buffer
 * @param delta the number of bytes needed (less than 0), or the number of
 *              bytes used (greater than 0), or 0 on error
 *
 * @return  0 if successful, error otherwise
 *         -1 reserved bits are not 0
 *         -2 invalid/unrecognized opcode
 *         -3 length field was too small (<126) for the length field size
 *         -4 length field was too large
 *         -5 length field was too small (<65536) for the length field size
 */
int frame_decode(struct cws_frame *f, const void *buf, size_t len, ssize_t *delta);

/**
 * Takes a frame, payload and buffer (that MUST be larger than the frame plus
 * the payload) and output the WebSocket frame.
 *
 * @param f    the frame to encode
 * @param buf  the buffer to write to
 * @param len  the size of the buffer
 *
 * @return the number of bytes written to buf, or 0 on error
 */
size_t frame_encode(const struct cws_frame *f, void *buf, size_t len);

#endif

