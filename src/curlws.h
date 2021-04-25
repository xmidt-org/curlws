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
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Used to define the location in the stream this frame is by (*on_fragment) */
#define CWS_CONT        0x00000100
#define CWS_BINARY      0x00000200
#define CWS_TEXT        0x00000400

#define CWS_FIRST       0x01000000
#define CWS_LAST        0x02000000

/* The constant used to enable insecure mode. */
#define CURLWS_INSECURE_MODE 0x7269736b

/* All possible error codes from all the curlws functions. Future versions
 * may return other values.
 *
 * Always add new return codes last.  Do not remove any.  The return codes
 * must remain the same.
 */
typedef enum {
    CWSE_OK = 0,
    CWSE_OUT_OF_MEMORY,                 /*  1 */
    CWSE_CLOSED_CONNECTION,             /*  2 */
    CWSE_INVALID_CLOSE_REASON_CODE,     /*  3 */
    CWSE_APP_DATA_LENGTH_TOO_LONG,      /*  4 */
    CWSE_UNSUPPORTED_INTEGER_SIZE,      /*  5 */
    CWSE_INTERNAL_ERROR,                /*  6 */
    CWSE_INVALID_OPCODE,                /*  7 */
    CWSE_STREAM_CONTINUITY_ISSUE,       /*  8 */
    CWSE_INVALID_OPTIONS,               /*  9 */
    CWSE_INVALID_UTF8,                  /* 10 */
    CWSE_BAD_FUNCTION_ARGUMENT,         /* 11 */

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
     *      Sec-WebSocket-Extensions
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
    const char *interface;

    /* Set the verbosity level.  You hardly ever want this set in production
     * use, you will almost always want this when you debug/report problems.
     *
     * 3 - verbose curl + verbose library
     * 2 - verbose curl
     * 1 - verbose library
     * 0 - quiet
     */
    int verbose;

    /* Used to tell curlws to use the specified stream instead of the default
     * stderr stream for verbose output for curlws and the associated libcurl
     * debug if requested.
     */
    FILE *verbose_stream;

    /* Set the IP version to use when connecting.
     *  0 is system default
     *  4 forces IPv4
     *  6 forces IPv6
     */
    int ip_version;

    /* If set to CURLWS_INSECURE_MODE, this forces the connection establishment
     * to ignore hostname validation logic and other protective measures.
     * Functionally equivalent to using -k with the curl cli.  Any other value
     * defaults to the secure validation of host/peer names. */
    int insecure_ok;

    /* The various websocket protocols in a single string format.
     * Something like "chat", "superchat", "superchat,chat" ...
     * The default is no special protocols (NULL)
     */
    const char* websocket_protocols;

    /* If set to 1, send the 'Expect: 101' header that forces the server to send
     * a 101 vs possibly a 100.  Some server implementations reject this
     * option with a 417.
     */
    int expect;

    /* The largest amount of payload data sent as one websocket frame.
     * If set to 0 the library default of 1024 will be used.
     */
    size_t max_payload_size;

    /**
     * Called upon connection, websocket_protocols contains what
     * server reported as 'Sec-WebSocket-Protocol:'.
     *
     * @note It is not validated if matches the proposed protocols.
     *
     * @param user      the user data specified in this configuration
     * @param handle    handle for this websocket
     * @param protocols the websocket protocols from the server
     */
    void (*on_connect)(void *user, CWS *handle, const char *protocols);

    /**
     * Reports UTF-8 text messages.
     *
     * @note The text field is guaranteed to be NULL (\0) terminated, but the
     *       UTF-8 is not validated. If it's invalid, consider closing the
     *       connection with code = 1007.
     *
     * @note If (*on_fragment) is set, this callback behavior is disabled.
     *
     * @param user   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param text   the text string being returned
     * @param len    the length of the text string (include trailing '\0')
     */
    void (*on_text)(void *user, CWS *handle, const char *text, size_t len);

    /**
     * Reports binary data.
     *
     * @note If (*on_fragment) is set, this callback behavior is disabled.
     *
     * @param user   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param buffer the data being returned
     * @param len    the length of the data
     */
    void (*on_binary)(void *user, CWS *handle, const void *buffer, size_t len);

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
     * @note The info parameter describes where this frame is in the stream of
     *       fragments and the type of the frame (Binary/Text/Continuation).
     *       For a stream with a single frame, both CWS_FIRST and CWS_LAST will
     *       both be set at.  All frames following the first frame will be of
     *       type CWS_CONT.  It is reasonable for a frame to not be either the
     *       first or last frame.
     *
     * @note If this callback is set to a non-NULL value, then the callbacks
     *       (*on_text) and (*on_binary) are disabled.
     *
     * @note Valid combinations of the info bitmask:
     *       CWS_BINARY | CWS_FIRST | CWS_LAST - a single binary fragment
     *
     *       CWS_BINARY | CWS_FIRST            - a binary fragment with
     *                                           additional fragments to follow
     *
     *       CWS_TEXT   | CWS_FIRST | CWS_LAST - a single text fragment
     *
     *       CWS_TEXT   | CWS_FIRST            - a text fragment with additional
     *                                           fragments to follow
     *
     *       CWS_CONT                          - a single fragment that is the
     *                                           type declared at the start of
     *                                           the stream of fragments, with
     *                                           additional fragments to follow
     *
     *       CWS_CONT | CWS_LAST               - a single fragment that is the
     *                                           type declared at the start of
     *                                           the stream of fragments, with
     *                                           no fragments to follow
     *
     * @param user   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param info   information about the frame of date presented.  Valid
     *               combinations are listed in the notes.
     *                 CWS_BINARY | CWS_TEXT |CWS_CONT (fragment type)
     *                 CWS_FIRST | CWS_LAST (if fragment is first, last or both)
     * @param buffer the data being returned (may be NULL)
     * @param len    the length of the data (may be 0)
     */
    void (*on_fragment)(void *user, CWS *handle, int info, const void *buffer, size_t len);

    /**
     * Reports PING.
     *
     * @note If provided you MUST reply with cws_pong(). The default behavior
     *       automatically sends a PONG message.
     *
     * @param user   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param buffer the data provided in the ping message
     * @param len    the length of the data
     */
    void (*on_ping)(void *user, CWS *handle, const void *buffer, size_t len);

    /**
     * Reports PONG.
     *
     * @param user   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param buffer the data provided in the pong message
     * @param len    the length of the data
     */
    void (*on_pong)(void *user, CWS *handle, const void *buffer, size_t len);

    /**
     * Reports server closed the connection with the given reason.
     *
     * @note Clients should not transmit any more data after the server is
     *       closed.
     *
     * @param user   the user data specified in this configuration
     * @param handle handle for this websocket
     * @param code   the websocket close code.  See for details:
     *               https://tools.ietf.org/html/rfc6455#section-7.4.1
     * @param reason the optional reason text (could be NULL)
     * @param len    the length of the optional reason (could be 0)
     */
    void (*on_close)(void *user, CWS *handle, int code, const char *reason, size_t len);

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
    void (*configure)(void *user, CWS *handle, CURL *easy);

    /* User provided data that is passed into the callbacks via the data arg. */
    void *user;
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
 * @return the response from libcurl or CURLM_UNKNOWN_OPTION if the CWS handle
 *         is invalid
 */
CURLMcode cws_multi_add_handle(CWS *cws_handle, CURLM *multi_handle);


/**
 * Provides a way to remove the WebSocket easy handle from a multi session
 * without exposing the easy handle.
 *
 * @param cws_handle   the websocket handle to associate with the multi handle
 * @param multi_handle the multi handle to control the websocket
 *
 * @return the response from libcurl or CURLM_UNKNOWN_OPTION if the CWS handle
 *         is invalid
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
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
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
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
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
 * @note Normally this call appends the close frame to whatever existing
 *       transactions are queued and closes the connection after the close frame
 *       is sent.  If you wish to interrupt the stream and close immediately
 *       you can make the code value negative to indicate it is urgent to close
 *       the connection immediately.  To represent an urgent version of the '0'
 *       reason code (where no reason is sent), the value of '-1' should
 *       be used.
 *
 * @param handle the websocket handle to interact with
 * @param code   the reason code why it was closed, see the well-known numbers
 *               at: https://tools.ietf.org/html/rfc6455#section-7.4.1
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
 * @retval CWSE_INVALID_UTF8
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
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
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
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
 * @retval CWSE_INVALID_UTF8
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
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
 *               information is ignored.  Set the CWS_FIRST and/or CWS_LAST to
 *               indicate if the frame is the first or last in a sequence.
 * @param data   the buffer to send
 * @param len    the number of bytes in the buffer
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_STREAM_CONTINUITY_ISSUE
 * @retval CWSE_INVALID_OPTIONS
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
 */
CWScode cws_send_strm_binary(CWS *handle, int info, const void *data, size_t len);


/**
 * Send a text (opcode 0x1) message of a given size.
 *
 * @note If the len is specified as something other than SIZE_MAX then no
 *       terminating '\0' is needed.  If SIZE_MAX is used then strlen() is
 *       used to determine the string length and a terminating '\0' is required.
 *
 * @note This interface will not validate multi-byte UTF-8 characters split
 *       across buffer boundaries.  Either you should align the buffer boundary
 *       to the character boundary or validate on prior to making this call.
 *
 * @param handle the websocket handle to interact with
 * @param info   information about the frame of date presented.  The type
 *               information is ignored.  Set the CWS_FIRST and/or CWS_LAST to
 *               indicate if the frame is the first or last in a sequence.
 * @param s      the text (UTF-8) to send.  If len = SIZE_MAX then strlen(s) is
 *               used to determine the length of the string.
 * @param len    the number of bytes in the buffer
 *
 * @retval CWSE_OK
 * @retval CWSE_OUT_OF_MEMORY
 * @retval CWSE_CLOSED_CONNECTION
 * @retval CWSE_STREAM_CONTINUITY_ISSUE
 * @retval CWSE_INVALID_OPTIONS
 * @retval CWSE_INVALID_UTF8
 * @retval CWSE_BAD_FUNCTION_ARGUMENT
 */
CWScode cws_send_strm_text(CWS *handle, int info, const char *s, size_t len);


/**
 * Provide a mechanism for software that includes this library to easily be in
 * compliance with the terms for the license & notice.
 *
 * @retval returns a string of the notice details that should not be free()d
 */
const char* cws_get_notice();

#ifdef __cplusplus
}
#endif

#endif
