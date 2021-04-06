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
/* c-mode: linux-4 */

#ifndef _CURLWS_H_
#define _CURLWS_H_ 1

#include <curl/curl.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WTS: I wonder if we should be silent on the reason codes... */
/* see https://tools.ietf.org/html/rfc6455#section-7.4.1 */
#define CWS_CLOSE_REASON_NORMAL                 1000
#define CWS_CLOSE_REASON_GOING_AWAY             1001
#define CWS_CLOSE_REASON_PROTOCOL_ERROR         1002
#define CWS_CLOSE_REASON_UNEXPECTED_DATA        1003
#define CWS_CLOSE_REASON_NO_REASON              1005
#define CWS_CLOSE_REASON_ABNORMALLY             1006
#define CWS_CLOSE_REASON_INCONSISTENT_DATA      1007
#define CWS_CLOSE_REASON_POLICY_VIOLATION       1008
#define CWS_CLOSE_REASON_TOO_BIG                1009
#define CWS_CLOSE_REASON_MISSING_EXTENSION      1010
#define CWS_CLOSE_REASON_SERVER_ERROR           1011
#define CWS_CLOSE_REASON_TLS_HANDSHAKE_ERROR    1015

/* Used to define the location in the stream this frame is by (*on_stream) */
#define CWS_CONT        0x000100
#define CWS_BINARY      0x000200
#define CWS_TEXT        0x000400
#define CWS_FIRST_FRAME 0x010000
#define CWS_LAST_FRAME  0x020000

/* All possible error codes from all the curlws functions. Future versions
 * may return other values.
 *
 * Always add new return codes last.  Do not remove any.  The return codes
 * must remain the same.
 */
typedef enum {
    CWSE_OK = 0,
    CWSE_OUT_OF_MEMORY,                 /* 1 */
    CWSE_CLOSED_CONNECTION,             /* 2 */
    CWSE_INVALID_CLOSE_REASON_CODE,     /* 3 */
    CWSE_APP_DATA_LENGTH_TOO_LONG,      /* 4 */
    CWSE_UNSUPPORTED_INTEGER_SIZE,      /* 5 */
    CWSE_INTERNAL_ERROR,                /* 6 */
    CWSE_INVALID_OPCODE,                /* 7 */
    CWSE_STREAM_CONTINUITY_ISSUE,       /* 8 */
    CWSE_INVALID_OPTIONS,               /* 9 */

    CWSE_LAST /* never use! */
} CWScode;


/**
 * The curlws base object type that is designed to be opaque, but passed
 * around.
 */
typedef struct cws_object CWS;


/**
 * The curlws configuration struct.  If initially filled with zeros and the
 * URL is added, this should give a reasonable set of defaults.
 */
struct cws_config {

    /* The initial URL to connect to. */
    const char *url;

    /* Any extra headers that you wish to send along with the connection
     * request(s) or NULL otherwise.
     *
     * Do not specify any of the following headers:
     *      Connection
     *      Expect
     *      Sec-WebSocket-Accept
     *      Sec-WebSocket-Key
     *      Sec-WebSocket-Protocol
     *      Sec-WebSocket-Version
     *      Transfer-Encoding
     *      Upgrade
     */
    struct curl_slist *extra_headers;

    /* The number of redirections to follow. -1 for infinite, 0 for none, 1+
     * for a specific maximum number. */
    long max_redirects;

    /* Sets the interface name to use as the outgoing network interface.
     * Follows these rules if not NULL:
     *      https://curl.se/libcurl/c/CURLOPT_INTERFACE.html
     */
    const char *network_interface;

    /* Set the verbosity level.  You hardly ever want this set in production
     * use, you will almost always want this when you debug/report problems.
     *
     * 3 - verbose curl + verbose library
     * 2 - verbose curl
     * 1 - verbose library
     * 0 - quiet
     */
    int verbose;

    /* Set the IP version to use when connecting.
     *  0 is system default
     *  4 forces IPv4
     *  6 forces IPv6
     */
    int ip_version;

    /* If set, this forces the connection establishment to ignore hostname
     * validation logic and other protective measures.  Funcationally equal
     * to using -k with the curl cli. */
    /* WTS: TODO */

    /* The various websocket protocols in a single string format.
     * Something like "chat", "superchat", "superchat,chat" ...
     * The default is no special protocols (NULL)
     */
    const char* websocket_protocols;

    /* If set to 1, send the 'Expect: 101' header that forces the server to send
     * a 101 vs possibly a 100.  Some server implementations reject this
     * option with a 417.
     */
    int explicit_expect;

    /* The largest amount of payload data sent as one websocket frame.
     * If set to 0 the library default of 1024 will be used.
     */
    size_t max_payload_size;

    /**
     * Called upon connection, websocket_protocols contains what
     * server reported as 'Sec-WebSocket-Protocol:'.
     *
     * @note It is not validated if matches the proposed protocols.
     */
    void (*on_connect)(void *data, CWS *handle, const char *websocket_protocols);

    /**
     * Reports UTF-8 text messages.
     *
     * @note The text field is guaranteed to be NULL (\0) terminated, but the
     *       UTF-8 is not validated. If it's invalid, consider closing the
     *       connection with #CWS_CLOSE_REASON_INCONSISTENT_DATA.
     *
     * @param data   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param text   the text string being returned
     * @param len    the length of the text string (include trailing '\0')
     */
    void (*on_text)(void *data, CWS *handle, const char *text, size_t len);

    /**
     * Reports binary data.
     *
     * @param data   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param mem    the data being returned
     * @param len    the length of the data
     */
    void (*on_binary)(void *data, CWS *handle, const void *mem, size_t len);

    /**
     * Reports data in a steaming style of interface for the non-control
     * messages.
     *
     * @note Control messages may be interleved with the data messages as this
     *       is part of the websocket specification.
     *       https://tools.ietf.org/html/rfc6455#section-5.4
     *
     * @note No control message data will be reported using this function call.
     *
     * @note The info parameter describes where this frame is in the stream,
     *       and the type of the frame (Binary/Text).  For a stream with a
     *       single frame, both CWS_FIRST_FRAME and CWS_LAST_FRAME will be set.
     *       It is reasonable for a frame to not be either the first or last
     *       frame
     *
     * @note If this callback is set to a non-NULL value, then the callbacks
     *       (*on_text) and (*on_binary) are disabled.
     *
     * @param data   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param info   information about the frame of date presented
     *                  Either CWS_BINARY or CWS_TEXT will be set indicating
     *                  the type of the payload.
     *                  CWS_FIRST_FRAME will be set if this is the first frame
     *                  in a sequence.
     *                  CWS_LAST_FRAME will be set if this is the last frame in
     *                  a sequence.
     * @param mem    the data being returned
     * @param len    the length of the data
     *
     * with #CWS_CLOSE_REASON_INCONSISTENT_DATA.
     */
    void (*on_stream)(void *data, CWS *handle, int info, const void *mem, size_t len);

    /**
     * Reports PING.
     *
     * @note if provided you should reply with cws_pong(). If not
     * provided, pong is sent with the same message payload.
     */
    void (*on_ping)(void *data, CWS *handle, const void *mem, size_t len);

    /**
     * Reports PONG.
     */
    void (*on_pong)(void *data, CWS *handle, const void *mem, size_t len);

    /**
     * Reports server closed the connection with the given reason.
     *
     * Clients should not transmit any more data after the server is
     * closed, just call cws_free().
     */
    void (*on_close)(void *data, CWS *handle, int code, const char *reason, size_t len);

    /**
     * Provides a way to specifiy a custom random generator instead of the
     * default provided one.
     *
     * Fill the 'buffer' of length 'len' bytes with random data & return.
     */
    void (*get_random)(void *data, CWS *handle, void *buffer, size_t len);

    /**
     * Provides a way to specifiy a custom debug handler.
     *
     * WTS TODO XXX.. usure if I need or want this.
     */
    void (*debug)(void *data, CWS *handle, const char *format, ...);

    /**
     * ----------------------------------------------------------------------
     * WARNING!  Using this method for configuration can result in unexpected
     * results.  Use only if needed.
     * ----------------------------------------------------------------------
     *
     * Called to enable the configuration of the connection using uncommonly
     * needed curl_easy_setopt() based options.  Generally do not use the
     * following options or options that interfere with their behavior:
     *      CURLOPT_HEADER, 
     *      CURLOPT_WILDCARDMATCH,
     *      CURLOPT_NOSIGNAL,
     *      CURLOPT_HEADERDATA,
     *      CURLOPT_HEADERFUNCTION,
     *      CURLOPT_WRITEDATA
     *      CURLOPT_WRITEFUNCTION
     *      CURLOPT_READDATA
     *      CURLOPT_READFUNCTION
     *      CURLOPT_TIMEOUT
     *      CURLOPT_URL
     *      CURLOPT_HTTP_VERSION
     *      CURLOPT_UPLOAD
     *      CURLOPT_CUSTOMREQUEST
     *      CURLOPT_HTTPHEADER
     *
     * ----------------------------------------------------------------------
     * You are responsible for ensuring the proper behavior of any settings
     * specified via this interface!  You have been warned!
     * ----------------------------------------------------------------------
     */
    void (*configure)(void *data, CWS *handle, CURL *easy);

    /* User provided data that is passed into the callbacks via the data arg. */
    void *data;
};


/*----------------------------------------------------------------------------*/
/*                               Lifecycle APIs                               */
/*----------------------------------------------------------------------------*/


/**
 * Create a new CURL-based WebSocket handle.
 *
 * This is a regular CURL easy handle properly setup to do
 * WebSocket. You can add more headers and cookies, but do @b not mess
 * with the following headers:
 *  @li Content-Length
 *  @li Content-Type
 *  @li Transfer-Encoding
 *  @li Connection
 *  @li Upgrade
 *  @li Expect
 *  @li Sec-WebSocket-Version
 *  @li Sec-WebSocket-Key
 *
 * And do not change the HTTP method or version, callbacks (read,
 * write or header) or private data.
 *
 * @param config the configuration structure
 *
 * @return newly created curl websocket handle, free with cws_destroy()
 */
CWS *cws_create(const struct cws_config *config);


/**
 * Provides a way to add the WebSocket easy handle to a multi session without
 * exposing the easy handle.
 *
 * @param cws_handle   the websocket handle to associate with the multi handle
 * @param multi_handle the multi handle to control the websocket
 *
 * @return the response from libcurl
 */
CURLMcode cws_multi_add_handle(CWS *cws_handle, CURLM *multi_handle);


/**
 * Provides a way to remove the WebSocket easy handle from a multi session
 * without exposing the easy handle.
 *
 * @param cws_handle   the websocket handle to associate with the multi handle
 * @param multi_handle the multi handle to control the websocket
 *
 * @return the response from libcurl
 */
CURLMcode cws_multi_remove_handle(CWS *cws_handle, CURLM *multi_handle);


/**
 * Frees a handle and all resources created with cws_create()
 *
 * @param handle the websocket handle to destroy
 */
void cws_destroy(CWS *handle);


/*----------------------------------------------------------------------------*/
/*                                 Control APIs                               */
/*----------------------------------------------------------------------------*/


/**
 * Send a PING (opcode 0x9) frame with an optional reason as payload.
 *
 * @note The application data is limited to 125 bytes or less.
 *       See https://tools.ietf.org/html/rfc6455#section-5.5 for details.
 *
 * @param handle the websocket handle to interact with
 * @param data   Application data to send back.  Limited to 125 or less.
 * @param len    the length of data in bytes
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_APP_DATA_LENGTH_TOO_LONG
 */
CWScode cws_ping(CWS *handle, const void *data, size_t len);


/**
 * Send a PONG (opcode 0xA) frame with an optional reason as payload.
 *
 * @note A pong is sent automatically if no "on_ping" callback is defined.
 *       If one is defined you must send pong manually.
 *
 * @note The response for a PING frame MUST contain identical application data.
 *       See https://tools.ietf.org/html/rfc6455#section-5.5.2 for details.
 *
 * @note The application data is limited to 125 bytes or less.
 *       See https://tools.ietf.org/html/rfc6455#section-5.5 for details.
 *
 * @param handle the websocket handle to interact with
 * @param data   Application data to send back.  Limited to 125 or less.
 * @param len    the length of data in bytes
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_APP_DATA_LENGTH_TOO_LONG
 */
CWScode cws_pong(CWS *handle, const void *data, size_t len);


/**
 * Send a CLOSE (opcode 0x8) frame with an optional reason as payload.
 *
 * @note If code == 0 && reason == NULL && len = 0 then no code is sent.
 *       Otherwise the code must be valid and is sent.
 *
 * @note The application data is limited to 125 bytes or less.  The CLOSE
 *       opcode requires that the first 2 bytes of application data be the
 *       close reason code.  This means the reason string is limited to 123
 *       bytes.
 *       See https://tools.ietf.org/html/rfc6455#section-5.5 for details.
 *       See https://tools.ietf.org/html/rfc6455#section-5.5.1 for details.
 *
 * @param handle the websocket handle to interact with
 * @param code   the reason code why it was closed, see the well-known numbers.
 * @param reason #NULL or some UTF-8 string null ('\0') terminated.
 * @param len    the length of reason in bytes. If SIZE_MAX is used,
 *               strlen() is used on reason (if not NULL).  Limited to 123 or
 *               less.
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_INVALID_CLOSE_REASON_CODE
 * @retval CWSE_APP_DATA_LENGTH_TOO_LONG
 */
CWScode cws_close(CWS *handle, int code, const char *reason, size_t len);


/*----------------------------------------------------------------------------*/
/*                              Block Based APIs                              */
/*----------------------------------------------------------------------------*/


/**
 * Send a binary (opcode 0x2) message of a given size.
 *
 * @param handle the websocket handle to interact with
 * @param data   the buffer to send
 * @param len    the number of bytes in the buffer
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 */
CWScode cws_send_blk_binary(CWS *handle, const void *data, size_t len);


/**
 * Send a text (opcode 0x1) message of a given size.
 *
 * @note If the len is specified as something other than SIZE_MAX then no
 *       terminating '\0' is needed.  If SIZE_MAX is used then strlen() is
 *       used to determine the string length and a terminating '\0' is required.
 *
 * @param handle the websocket handle to interact with
 * @param s      the text (UTF-8) to send.  If len = SIZE_MAX then strlen(s) is
 *               used to determine the length of the string.
 * @param len    the number of bytes in the buffer
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 */
CWScode cws_send_blk_text(CWS *handle, const char *s, size_t len);


/*----------------------------------------------------------------------------*/
/*                              Stream Based APIs                             */
/*----------------------------------------------------------------------------*/


/**
 * Send a binary (opcode 0x2) message of a given size.
 *
 * 
 * @param handle the websocket handle to interact with
 * @param info   information about the frame of date presented.  The type
 *               information is ignored.  Set the CWS_FIRST_FRAME and/or
 *               CWS_LAST_FRAME to indicate if the frame is the first or last
 *               in a sequence.
 * @param data   the buffer to send
 * @param len    the number of bytes in the buffer
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_STREAM_CONTINUITY_ISSUE
 */
CWScode cws_send_strm_binary(CWS *handle, int info, const void *data, size_t len);


/**
 * Send a text (opcode 0x1) message of a given size.
 *
 * @note If the len is specified as something other than SIZE_MAX then no
 *       terminating '\0' is needed.  If SIZE_MAX is used then strlen() is
 *       used to determine the string length and a terminating '\0' is required.
 *
 * @param handle the websocket handle to interact with
 * @param info   information about the frame of date presented.  The type
 *               information is ignored.  Set the CWS_FIRST_FRAME and/or
 *               CWS_LAST_FRAME to indicate if the frame is the first or last
 *               in a sequence.
 * @param s      the text (UTF-8) to send.  If len = SIZE_MAX then strlen(s) is
 *               used to determine the length of the string.
 * @param len    the number of bytes in the buffer
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_STREAM_CONTINUITY_ISSUE
 */
CWScode cws_send_strm_text(CWS *handle, int info, const char *s, size_t len);


#ifdef __cplusplus
}
#endif

#endif
