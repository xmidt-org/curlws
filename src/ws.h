/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __WS_H__
#define __WS_H__

#include <stdbool.h>

/*----------------------------------------------------------------------------*/
/*                     Websocket based macros/definitions                     */
/*----------------------------------------------------------------------------*/

/* The key header that the websocket expects to get back.  The size is based on:
 * b64(20 bytes) = 28 bytes + 1 for '\0' */
#define WS_HTTP_EXPECTED_KEY_SIZE   29
/*
 *
 * WebSocket is a framed protocol in the format:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-------+-+-------------+-------------------------------+
 *   |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *   |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 *   |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *   | |1|2|3|       |K|             |                               |
 *   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 *   |     Extended payload length continued, if payload len == 127  |
 *   + - - - - - - - - - - - - - - - +-------------------------------+
 *   |                               |Masking-key, if MASK set to 1  |
 *   +-------------------------------+-------------------------------+
 *   | Masking-key (continued)       |          Payload Data         |
 *   +-------------------------------- - - - - - - - - - - - - - - - +
 *   :                     Payload Data continued ...                :
 *   + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 *   |                     Payload Data continued ...                |
 *   +---------------------------------------------------------------+
 *
 * See https://tools.ietf.org/html/rfc6455#section-5.2
 *
 * The goal isn't to try to define every possible data model as a macro, but
 * provide the really useful and commonly needed values.
 *
 */

/* This covers the "base" frame size including the small 7 bit length and
 * no mask. */
#define WS_FRAME_HEADER_MIN                 2

/* The mask is defined as 4 bytes (after the payload length) */
#define WS_FRAME_HEADER_MASK                4

/* The largest length is a uin64_t (8 bytes) */
#define WS_FRAME_HEADER_MAX_PAYLOAD_SIZE    8

/* The largest possible frame header */
#define WS_FRAME_HEADER_MAX                 (WS_FRAME_HEADER_MIN + \
                                             WS_FRAME_HEADER_MASK + \
                                             WS_FRAME_HEADER_MAX_PAYLOAD_SIZE)

/* The control frames have a limited payload length define here:
 * See https://tools.ietf.org/html/rfc6455#section-5.5  */
#define WS_CTL_PAYLOAD_MAX 125

/* The largest control frame and payload in bytes */
#define WS_CTL_FRAME_MAX   (WS_FRAME_HEADER_MIN + \
                            WS_FRAME_HEADER_MASK + \
                            WS_CTL_PAYLOAD_MAX)

/* The Websocket Opcodes */
#define WS_OPCODE_CONTINUATION  0x0
#define WS_OPCODE_TEXT          0x1
#define WS_OPCODE_BINARY        0x2
#define WS_OPCODE_CLOSE         0x8
#define WS_OPCODE_PING          0x9
#define WS_OPCODE_PONG          0xa

/**
 * Returns if the close code is valid.
 */
bool is_close_code_valid(int code);

#endif
