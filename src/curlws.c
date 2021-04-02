/*
 * Copyright (C) 2016 Gustavo Sverzut Barbieri
 * Copyright (c) 2021  Comcast Cable Communications Management, LLC
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
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "curlws.h"
#include "utils.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */
#define IGNORE_UNUSED(x) (void) x;

#define STR_OR_EMPTY(p) (p != NULL ? p : "")

/* Temporary buffer size to use during WebSocket masking.
 * stack-allocated
 */
#define CWS_MASK_TMPBUF_SIZE 4096

#define WS_CTL_FRAME_MAX (125)

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
enum cws_opcode {
    CWS_OPCODE_CONTINUATION = 0x0,
    CWS_OPCODE_TEXT         = 0x1,
    CWS_OPCODE_BINARY       = 0x2,
    CWS_OPCODE_CLOSE        = 0x8,
    CWS_OPCODE_PING         = 0x9,
    CWS_OPCODE_PONG         = 0xa,
};


/*
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
 */
struct cws_frame_header {
    /* first byte: fin + opcode */
    uint8_t opcode : 4;
    uint8_t _reserved : 3;
    uint8_t fin : 1;

    /* second byte: mask + payload length */
    uint8_t payload_len : 7; /* if 126, uses extra 2 bytes (uint16_t)
                              * if 127, uses extra 8 bytes (uint64_t)
                              * if <=125 is self-contained
                              */
    uint8_t mask : 1; /* if 1, uses 4 extra bytes */
};

struct cws_frame {
    uint8_t fin        : 1;
    uint8_t mask       : 1;
    uint8_t is_control : 1;
    uint8_t opcode     : 4;

    uint32_t masking_key;

    uint64_t payload_len;
    const void *payload;
    const void *next;
};

struct cws_object {
    char *url;
    CURL *easy;

    void (*on_connect_fn)(void*, CWS*, const char*);
    void (*on_text_fn)(void*, CWS*, const char*, size_t);
    void (*on_binary_fn)(void*, CWS*, const void*, size_t);
    void (*on_ping_fn)(void*, CWS*, const char*, size_t);
    void (*on_pong_fn)(void*, CWS*, const char*, size_t);
    void (*on_close_fn)(void*, CWS*, int, const char*, size_t);
    void (*get_random_fn)(void*, CWS*, void*, size_t);
    void (*debug_fn)(void*, CWS*, const char*, ...);
    void (*configure_fn)(void*, CWS*, CURL*);

    void *user;

    struct {
        char *requested;
        char *received;
    } websocket_protocols;

    struct curl_slist *headers;

    char *expected_key_header;

    struct {
        struct {
            uint8_t *payload;
            uint64_t used;
            uint64_t total;
            enum cws_opcode opcode;
            bool fin;
        } current;
        struct {
            uint8_t *payload;
            uint64_t used;
            uint64_t total;
            enum cws_opcode opcode;
        } fragmented;

        uint8_t tmpbuf[sizeof(struct cws_frame_header) + sizeof(uint64_t)];
        uint8_t done; /* of tmpbuf, for header */
        uint8_t needed; /* of tmpbuf, for header */
    } recv;
    struct {
        uint8_t *buffer;
        size_t len;
    } send;
    uint8_t dispatching;
    uint8_t pause_flags;
    bool accepted;
    bool upgraded;
    bool connection_websocket;
    bool closed;
    bool deleted;
};

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static bool _cws_opcode_is_control(enum cws_opcode);
static int _is_valid_close_to_server(int);
static int _is_valid_close_from_server(int);
static void _cws_check_accept(CWS*, const char*, size_t);
static void _cws_check_protocol(CWS*, const char*, size_t);
static void _cws_check_upgrade(CWS*, const char*, size_t);
static void _cws_check_connection(CWS*, const char*, size_t);
static size_t _cws_receive_header(const char*, size_t, size_t, void*);
static bool _cws_dispatch_validate(CWS *priv);
static void _cws_dispatch(CWS*);
static size_t _cws_process_frame(CWS*, const char*, size_t);
static size_t _cws_receive_data(const char*, size_t, size_t, void*);
static size_t _cws_send_data(char*, size_t, size_t, void*);
static CWScode _cws_write(CWS*, const void*, size_t);
static CWScode _cws_write_masked(CWS*, const uint8_t*, const void*, size_t);
static CWScode _cws_send(CWS*, enum cws_opcode, const void*, size_t);
static void _cws_cleanup(CWS*);

static bool _validate_config(const struct cws_config*);
static bool _cws_calculate_websocket_key(CWS*);
static bool _cws_add_websocket_protocols(CWS*, const char *);
static bool _cws_add_extra_headers(CWS*, const struct cws_config*);
static void _populate_callbacks(CWS*, const struct cws_config *src);
static void _default_on_connect(void*, CWS*, const char*);
static void _default_on_text(void*, CWS*, const char*, size_t);
static void _default_on_binary(void*, CWS*, const void*, size_t);
static void _default_on_ping(void*, CWS*, const void*, size_t);
static void _default_on_pong(void*, CWS*, const void*, size_t);
static void _default_on_close(void*, CWS*, int, const char*, size_t);
static void _default_get_random(void*, CWS*, void*, size_t);
static void _default_configure(void*, CWS*, CURL*);
static void _default_debug(void*, CWS*, const char*, ...);
static void _default_verbose_debug(void*, CWS*, const char*, ...);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

CWS *cws_create(const struct cws_config *config)
{
    CWS *priv = NULL;
    const curl_version_info_data *curl_ver = NULL;
    struct curl_slist *tmp = NULL;

    curl_ver = curl_version_info(CURLVERSION_NOW);

    /* TODO figure out how to make this a compile/link time check ... */
    if (curl_ver->version_num < 0x073202) {
        fprintf(stderr, "ERROR: CURL version '%s'. At least '7.50.2' is required for WebSocket to work reliably", curl_ver->version);
        goto error;
    }

    if (!_validate_config(config)) {
        goto error;
    }

    priv = (CWS*) calloc(1, sizeof(struct cws_object));
    if (!priv) { goto error; }

    _populate_callbacks(priv, config);

    priv->easy = curl_easy_init();
    if (!priv->easy) { goto error; }

    priv->user = config->data;

    /* Place things here that are "ok" for a user to overwrite. */

    /* Force a reasonably modern version of TLS */
    curl_easy_setopt(priv->easy, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    priv->configure_fn(priv->user, priv, priv->easy);

    curl_easy_setopt(priv->easy, CURLOPT_PRIVATE, priv);
    curl_easy_setopt(priv->easy, CURLOPT_HEADERFUNCTION, _cws_receive_header);
    curl_easy_setopt(priv->easy, CURLOPT_HEADERDATA, priv);
    curl_easy_setopt(priv->easy, CURLOPT_WRITEFUNCTION, _cws_receive_data);
    curl_easy_setopt(priv->easy, CURLOPT_WRITEDATA, priv);
    curl_easy_setopt(priv->easy, CURLOPT_READFUNCTION, _cws_send_data);
    curl_easy_setopt(priv->easy, CURLOPT_READDATA, priv);

    priv->recv.needed = sizeof(struct cws_frame_header);
    priv->recv.done = 0;

    priv->url = cws_rewrite_url(config->url);
    if (!priv->url) { goto error; }
    curl_easy_setopt(priv->easy, CURLOPT_URL, priv->url);

    if (0 != (2 & config->verbose)) {
        curl_easy_setopt(priv->easy, CURLOPT_VERBOSE, 1L);
    }

    /*
     * BEGIN: work around CURL to get WebSocket:
     *
     * WebSocket must be HTTP/1.1 GET request where we must keep the
     * "send" part alive without any content-length and no chunked
     * encoding and the server answer is 101-upgrade.
     */
    curl_easy_setopt(priv->easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    /* Use CURLOPT_UPLOAD=1 to force "send" even with a GET request,
     * however it will set HTTP request to PUT
     */
    curl_easy_setopt(priv->easy, CURLOPT_UPLOAD, 1L);
    /*
     * Then we manually override the string sent to be "GET".
     */
    curl_easy_setopt(priv->easy, CURLOPT_CUSTOMREQUEST, "GET");
    /*
     * CURLOPT_UPLOAD=1 with HTTP/1.1 implies:
     *     Expect: 100-continue
     * but we don't want that, rather 101. Then force: 101.
     */
    if (1 == config->explicit_expect) {
        tmp = curl_slist_append(priv->headers, "Expect: 101");
        if (!tmp) { goto error; }
        priv->headers = tmp;
    }

    /*
     * CURLOPT_UPLOAD=1 without a size implies in:
     *     Transfer-Encoding: chunked
     * but we don't want that, rather unmodified (raw) bites as we're
     * doing the websockets framing ourselves. Force nothing.
     */
    tmp = curl_slist_append(priv->headers, "Transfer-Encoding:");
    if (!tmp) { goto error; }
    priv->headers = tmp;

    /* END: work around CURL to get WebSocket. */

    /* regular mandatory WebSockets headers */
    tmp = curl_slist_append(priv->headers, "Connection: Upgrade");
    if (!tmp) { goto error; }
    priv->headers = tmp;

    tmp = curl_slist_append(priv->headers, "Upgrade: websocket");
    if (!tmp) { goto error; }
    priv->headers = tmp;

    tmp = curl_slist_append(priv->headers, "Sec-WebSocket-Version: 13");
    if (!tmp) { goto error; }
    priv->headers = tmp;

    /* Calculate the unique key pair. */
    if (!_cws_calculate_websocket_key(priv)) {
        goto error;
    }


    if (!_cws_add_websocket_protocols(priv, config->websocket_protocols)) {
        goto error;
    }

    if (!_cws_add_extra_headers(priv, config)) {
        goto error;
    }

    curl_easy_setopt(priv->easy, CURLOPT_HTTPHEADER, priv->headers);

    return priv;

error:
    cws_destroy(priv);
    return NULL;
}


void cws_destroy(CWS *priv)
{
    if (priv) {
        priv->deleted = true;
        _cws_cleanup(priv);
    }
}


CWScode cws_send_binary(CWS *priv, const void *data, size_t len)
{
    if (!data || !len) {
        data = NULL;
        len = 0;
    }

    return _cws_send(priv, CWS_OPCODE_BINARY, data, len);
}


CWScode cws_send_text(CWS *priv, const char *s, size_t len)
{
    if (s) {
        if (len == SIZE_MAX) {
            len = strlen(s);
        }
    } else {
        len = 0;
    }

    return _cws_send(priv, CWS_OPCODE_TEXT, s, len);
}


CWScode cws_ping(CWS *priv, const void *data, size_t len)
{
    if (!data || !len) {
        data = NULL;
        len = 0;
    }

    if (WS_CTL_FRAME_MAX < len) {
        return CWSE_APP_DATA_LENGTH_TOO_LONG;
    }

    return _cws_send(priv, CWS_OPCODE_PING, data, len);
}


CWScode cws_pong(CWS *priv, const void *data, size_t len)
{
    if (!data || !len) {
        data = NULL;
        len = 0;
    }

    if (WS_CTL_FRAME_MAX < len) {
        return CWSE_APP_DATA_LENGTH_TOO_LONG;
    }

    return _cws_send(priv, CWS_OPCODE_PONG, data, len);
}


CWScode cws_close(CWS *priv, int code, const char *reason, size_t len)
{
    CWScode ret;
    uint8_t buf[WS_CTL_FRAME_MAX];  /* Limited by RFC6455 Section 5.5 */

    if ((0 == code) && (NULL == reason) && (0 == len)) {
        ret = _cws_send(priv, CWS_OPCODE_CLOSE, NULL, 0);
        priv->closed = true;
        return ret;
    }

    if (0 != _is_valid_close_to_server(code)) {
        return CWSE_INVALID_CLOSE_REASON_CODE;
    }

    if (reason) {
        if (len == SIZE_MAX) {
            len = strlen(reason);
        }
    } else {
        len = 0;
    }

    if ((sizeof(buf) - 2) < len) {
        return CWSE_APP_DATA_LENGTH_TOO_LONG;
    }

    buf[0] = (uint8_t) (0x00ff & (code >> 8));
    buf[1] = (uint8_t) (0x00ff & code);
    
    if (len) {
        memcpy(&buf[2], reason, len);
    }

    ret = _cws_send(priv, CWS_OPCODE_CLOSE, buf, (2+len));
    priv->closed = true;
    return ret;
}


CURLMcode cws_multi_add_handle(CWS *ws, CURLM *multi_handle)
{
    return curl_multi_add_handle(multi_handle, ws->easy);
}


CURLMcode cws_multi_remove_handle(CWS *ws, CURLM *multi_handle)
{
    return curl_multi_remove_handle(multi_handle, ws->easy);
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

#ifdef __INCLUDE_TEST_DECODER__
/**
 *  Processes a byte stream into a frame structure in a portable way.
 *
 *  @param f    the frame pointer to fill in
 *  @param buf  the buffer to process
 *  @param len  the number of bytes that are valid in the buffer
 *
 *  @return If greater than 0, the number of bytes we are short based on what
 *          we have so far.
 *          If 0 success.
 *          If less than zero, the frame is invalid.
 *              -1  reserved bits are not 0
 *              -2  invalid/unrecognized opcode
 *              -3  length field was too small (<126) for the length field size
 *              -4  length field was too large
 *              -5  length field was too small (<65536) for the length field size
 */
static ssize_t _cws_decode_frame( struct cws_frame *f, const void *buffer, size_t len )
{
#define INVALID 0
#define VALID   1
#define CONTROL 2

    const uint8_t valid_opcodes[16] = {
                  VALID, /* 0  - Continuation Frame     */
                  VALID, /* 1  - Text Frame             */
                  VALID, /* 2  - Binary Frame           */
                INVALID, /* 3  - (reserved)             */
                INVALID, /* 4  - (reserved)             */
                INVALID, /* 5  - (reserved)             */
                INVALID, /* 6  - (reserved)             */
                INVALID, /* 7  - (reserved)             */
        CONTROL | VALID, /* 8  - Connection Close Frame */
        CONTROL | VALID, /* 9  - Ping Frame             */
        CONTROL | VALID, /* 10 - Pong Frame             */
                INVALID, /* 11 - (reserved)             */
                INVALID, /* 12 - (reserved)             */
                INVALID, /* 13 - (reserved)             */
                INVALID, /* 14 - (reserved)             */
                INVALID  /* 15 - (reserved)             */      };

    const uint8_t *buf = (const uint8_t*) buffer;

    if (len < 2) {
        return 2 - len;
    }

    f->fin = 0;
    if (0x80 & (*buf)) {
        f->fin = 1;
    }

    if (0x70 & (*buf)) {
        return -1;
    }

    f->opcode = 0x0f & (*buf);
    if (INVALID == valid_opcodes[f->opcode]) {
        return -2;
    }
    f->is_control = 0;
    if (CONTROL == (CONTROL & valid_opcodes[f->opcode])) {
        f->is_control = 1;
    }
#undef INVALID
#undef VALID
#undef CONTROL

    buf++;
    len--;

    f->mask = 0;
    if (0x80 & (*buf)) {
        f->mask = 1;
    }

    f->payload_len = 0x7f & (*buf);

    buf++;
    len--;

    /* Deal with possible longer lengths */
    if (126 == f->payload_len) {
        if (len < 2) {
            return 2 - len;
        }
        f->payload_len = (0xff & buf[0]) << 8 | (0xff & buf[1]);
        buf += 2;
        len -= 2;

        if (f->payload_len < 126) {
            return -3;
        }
    } else if (127 == f->payload_len) {
        if (len < 8) {
            return 8 - len;
        }
        if (0x80 & buf[0]) {
            return -4;
        }
        f->payload_len = ((uint64_t) (0x7f & buf[0])) << 56 |
                         ((uint64_t) (0xff & buf[1])) << 48 |
                         ((uint64_t) (0xff & buf[2])) << 40 |
                         ((uint64_t) (0xff & buf[3])) << 32 |
                         ((uint64_t) (0xff & buf[4])) << 24 |
                         ((uint64_t) (0xff & buf[5])) << 16 |
                         ((uint64_t) (0xff & buf[6])) <<  8 |
                         ((uint64_t) (0xff & buf[7]));
        buf += 8;
        len -= 8;
        if (f->payload_len <= UINT16_MAX) {
            return -5;
        }
    }

    f->masking_key = 0;
    if (f->mask) {
        if (len < 4) {
            return 4 - len;
        }
        f->masking_key = (0xff & buf[0]) << 24 |
                         (0xff & buf[1]) << 16 |
                         (0xff & buf[2]) <<  8 |
                         (0xff & buf[3]);
        buf += 4;
        len -= 4;
    }

    if (len < f->payload_len) {
        return f->payload_len - len;
    }
    f->payload = buf;
    f->next = &buf[f->payload_len + 1];

    return 0;
}
#endif

static bool _cws_opcode_is_control(enum cws_opcode opcode)
{
    switch (opcode) {
    case CWS_OPCODE_CONTINUATION:
    case CWS_OPCODE_TEXT:
    case CWS_OPCODE_BINARY:
        return false;
    case CWS_OPCODE_CLOSE:
    case CWS_OPCODE_PING:
    case CWS_OPCODE_PONG:
        return true;
    }

    return true;
}


static int _is_valid_close_to_server(int code)
{
    if (((3000 <= code) && (code <= 4999)) ||
        ((1000 <= code) && (code <= 1003)) ||
        ((1007 <= code) && (code <= 1010)))
    {
        return 0;
    }

    return -1;
}


static int _is_valid_close_from_server(int code)
{
    if (((3000 <= code) && (code <= 4999)) ||
        ((1000 <= code) && (code <= 1003)) ||
        ((1007 <= code) && (code <= 1009)) || (1011 == code))
    {
        return 0;
    }

    return -1;
}


static void _cws_check_accept(CWS *priv, const char *buffer, size_t len)
{
    priv->accepted = false;

    if (len != strlen(priv->expected_key_header)) {
        (priv->debug_fn)(priv->user, priv,
                         "expected %zu bytes, got %zu '%.*s'",
                         strlen(priv->expected_key_header) - 1, len, (int)len, buffer);
        return;
    }

    if (memcmp(priv->expected_key_header, buffer, len) != 0) {
        (priv->debug_fn)(priv->user, priv,
                         "invalid accept key '%.*s', expected '%.*s'",
                         (int)len, buffer, (int)len, priv->expected_key_header);
        return;
    }

    priv->accepted = true;
}


static void _cws_check_protocol(CWS *priv, const char *buffer, size_t len)
{
    if (priv->websocket_protocols.received)
        free(priv->websocket_protocols.received);

    priv->websocket_protocols.received = cws_strndup(buffer, len);
}


static void _cws_check_upgrade(CWS *priv, const char *buffer, size_t len)
{
    priv->connection_websocket = false;

    if (len != strlen("websocket") ||
        cws_strncasecmp(buffer, "websocket", len) != 0)
    {
        (priv->debug_fn)(priv->user, priv,
                         "unexpected 'Upgrade: %.*s'. Expected 'Upgrade: websocket'",
                         (int)len, buffer);
        return;
    }

    priv->connection_websocket = true;
}


static void _cws_check_connection(CWS *priv, const char *buffer, size_t len)
{
    priv->upgraded = false;

    if (len != strlen("upgrade") ||
        cws_strncasecmp(buffer, "upgrade", len) != 0)
    {
        (priv->debug_fn)(priv->user, priv,
                         "unexpected 'Connection: %.*s'. Expected 'Connection: upgrade'",
                         (int)len, buffer);
        return;
    }

    priv->upgraded = true;
}

static size_t _cws_receive_header(const char *buffer, size_t count, size_t nitems, void *data)
{
    CWS *priv = data;
    size_t len = count * nitems;
    const struct header_checker {
        const char *prefix;
        void (*check)(CWS *priv, const char *suffix, size_t suffixlen);
    } *itr, header_checkers[] = {
        {"Sec-WebSocket-Accept:",   _cws_check_accept},
        {"Sec-WebSocket-Protocol:", _cws_check_protocol},
        {"Connection:",             _cws_check_connection},
        {"Upgrade:",                _cws_check_upgrade},
        {NULL, NULL}
    };

    if (len == 2 && memcmp(buffer, "\r\n", 2) == 0) {
        long status;

        curl_easy_getinfo(priv->easy, CURLINFO_HTTP_CONNECTCODE, &status);
        if (!priv->accepted) {
            priv->dispatching++;
            priv->on_close_fn(priv->user, priv,
                              CWS_CLOSE_REASON_SERVER_ERROR,
                              "server didn't accept the websocket upgrade",
                              strlen("server didn't accept the websocket upgrade"));
            priv->dispatching--;
            _cws_cleanup(priv);
            return 0;
        } else {
            priv->dispatching++;
            priv->on_connect_fn(priv->user, priv,
                                STR_OR_EMPTY(priv->websocket_protocols.received));
            priv->dispatching--;
            _cws_cleanup(priv);
            return len;
        }
    }

    if (cws_has_prefix(buffer, len, "HTTP/")) {
        priv->accepted = false;
        priv->upgraded = false;
        priv->connection_websocket = false;
        if (priv->websocket_protocols.received) {
            free(priv->websocket_protocols.received);
            priv->websocket_protocols.received = NULL;
        }
        return len;
    }

    for (itr = header_checkers; itr->prefix != NULL; itr++) {
        if (cws_has_prefix(buffer, len, itr->prefix)) {
            size_t prefixlen = strlen(itr->prefix);
            size_t valuelen = len - prefixlen;
            const char *value = buffer + prefixlen;
            value = cws_trim(value, &valuelen);
            itr->check(priv, value, valuelen);
        }
    }

    return len;
}

static bool _cws_dispatch_validate(CWS *priv)
{
    if (priv->closed && priv->recv.current.opcode != CWS_OPCODE_CLOSE)
        return false;

    if (!priv->recv.current.fin && _cws_opcode_is_control(priv->recv.current.opcode)) {
        (priv->debug_fn)(priv->user, priv,
                         "server sent forbidden fragmented control frame opcode=%#x.",
                         priv->recv.current.opcode);
    } else if (priv->recv.current.opcode == CWS_OPCODE_CONTINUATION && priv->recv.fragmented.opcode == 0) {
        (priv->debug_fn)(priv->user, priv,
                         "server sent continuation frame after non-fragmentable frame");
    } else {
        return true;
    }

    cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, NULL, 0);
    return false;
}

static void _cws_dispatch(CWS *priv)
{
    if (!_cws_dispatch_validate(priv))
        return;

    switch (priv->recv.current.opcode) {
    case CWS_OPCODE_CONTINUATION:
        if (priv->recv.current.fin) {
            if (priv->recv.fragmented.opcode == CWS_OPCODE_TEXT) {
                const char *str = (const char *)priv->recv.current.payload;
                if (priv->recv.current.used == 0)
                    str = "";
                priv->on_text_fn(priv->user, priv, str, priv->recv.current.used);
            } else if (priv->recv.fragmented.opcode == CWS_OPCODE_BINARY) {
                priv->on_binary_fn(priv->user, priv, priv->recv.current.payload, priv->recv.current.used);
            }
            memset(&priv->recv.fragmented, 0, sizeof(priv->recv.fragmented));
        } else {
            priv->recv.fragmented.payload = priv->recv.current.payload;
            priv->recv.fragmented.used = priv->recv.current.used;
            priv->recv.fragmented.total = priv->recv.current.total;
            priv->recv.current.payload = NULL;
            priv->recv.current.used = 0;
            priv->recv.current.total = 0;
        }
        break;

    case CWS_OPCODE_TEXT:
        if (priv->recv.current.fin) {
            const char *str = (const char *)priv->recv.current.payload;
            if (priv->recv.current.used == 0)
                str = "";
            priv->on_text_fn(priv->user, priv, str, priv->recv.current.used);
        } else {
            priv->recv.fragmented.payload = priv->recv.current.payload;
            priv->recv.fragmented.used = priv->recv.current.used;
            priv->recv.fragmented.total = priv->recv.current.total;
            priv->recv.fragmented.opcode = priv->recv.current.opcode;

            priv->recv.current.payload = NULL;
            priv->recv.current.used = 0;
            priv->recv.current.total = 0;
            priv->recv.current.opcode = 0;
            priv->recv.current.fin = 0;
        }
        break;

    case CWS_OPCODE_BINARY:
        if (priv->recv.current.fin) {
            priv->on_binary_fn(priv->user, priv, priv->recv.current.payload, priv->recv.current.used);
        } else {
            priv->recv.fragmented.payload = priv->recv.current.payload;
            priv->recv.fragmented.used = priv->recv.current.used;
            priv->recv.fragmented.total = priv->recv.current.total;
            priv->recv.fragmented.opcode = priv->recv.current.opcode;

            priv->recv.current.payload = NULL;
            priv->recv.current.used = 0;
            priv->recv.current.total = 0;
            priv->recv.current.opcode = 0;
            priv->recv.current.fin = 0;
        }
        break;

    case CWS_OPCODE_CLOSE: {
        uint16_t reason =  CWS_CLOSE_REASON_NO_REASON;
        const char *str = "";
        size_t len = priv->recv.current.used;

        if (priv->recv.current.used >= sizeof(uint16_t)) {
            uint16_t status;
            memcpy(&status, priv->recv.current.payload, sizeof(uint16_t));
            cws_ntoh(&status, sizeof(status));
            if (0 != _is_valid_close_from_server(status)) {
                cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, "invalid close reason", SIZE_MAX);
                status = CWS_CLOSE_REASON_PROTOCOL_ERROR;
            }
            reason = status;
            str = (const char *)priv->recv.current.payload + sizeof(uint16_t);
            len = priv->recv.current.used - 2;
        } else if (priv->recv.current.used > 0 && priv->recv.current.used < sizeof(uint16_t)) {
            cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, "invalid close payload length", SIZE_MAX);
        }

        priv->on_close_fn(priv->user, priv, reason, str, len);

        if (!priv->closed) {
            if (reason == CWS_CLOSE_REASON_NO_REASON)
                reason = 0;
            cws_close(priv, reason, str, len);
        }
        break;
    }

    case CWS_OPCODE_PING: {
        const char *str = (const char *)priv->recv.current.payload;
        if (priv->recv.current.used == 0)
            str = "";
        priv->on_ping_fn(priv->user, priv, str, priv->recv.current.used);
        break;
    }

    case CWS_OPCODE_PONG: {
        const char *str = (const char *)priv->recv.current.payload;
        if (priv->recv.current.used == 0)
            str = "";
        priv->on_pong_fn(priv->user, priv, str, priv->recv.current.used);
        break;
    }

    default:
        (priv->debug_fn)(priv->user, priv,
                         "unexpected WebSocket opcode: %#x.", priv->recv.current.opcode);
        cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, "unexpected opcode", SIZE_MAX);
    }
}

static size_t _cws_process_frame(CWS *priv, const char *buffer, size_t len)
{
    size_t used = 0;

    while (len > 0 && priv->recv.done < priv->recv.needed) {
        uint64_t frame_len;

        if (priv->recv.done < priv->recv.needed) {
            size_t todo = priv->recv.needed - priv->recv.done;
            if (todo > len)
                todo = len;
            memcpy(priv->recv.tmpbuf + priv->recv.done, buffer, todo);
            priv->recv.done += todo;
            used += todo;
            buffer += todo;
            len -= todo;
        }

        if (priv->recv.needed != priv->recv.done)
            continue;

        if (priv->recv.needed == sizeof(struct cws_frame_header)) {
            struct cws_frame_header fh;

            memcpy(&fh, priv->recv.tmpbuf, sizeof(struct cws_frame_header));
            priv->recv.current.opcode = fh.opcode;
            priv->recv.current.fin = fh.fin;

            if (fh._reserved || fh.mask)
                cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, NULL, 0);

            if (fh.payload_len == 126) {
                if (_cws_opcode_is_control(fh.opcode))
                    cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, NULL, 0);
                priv->recv.needed += sizeof(uint16_t);
                continue;
            } else if (fh.payload_len == 127) {
                if (_cws_opcode_is_control(fh.opcode))
                    cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, NULL, 0);
                priv->recv.needed += sizeof(uint64_t);
                continue;
            } else
                frame_len = fh.payload_len;
        } else if (priv->recv.needed == sizeof(struct cws_frame_header) + sizeof(uint16_t)) {
            uint16_t plen;

            memcpy(&plen,
                   priv->recv.tmpbuf + sizeof(struct cws_frame_header),
                   sizeof(plen));
            cws_ntoh(&plen, sizeof(plen));
            frame_len = plen;
        } else if (priv->recv.needed == sizeof(struct cws_frame_header) + sizeof(uint64_t)) {
            uint64_t plen;

            memcpy(&plen, priv->recv.tmpbuf + sizeof(struct cws_frame_header),
                   sizeof(plen));
            cws_ntoh(&plen, sizeof(plen));
            frame_len = plen;
        } else {
            (priv->debug_fn)(priv->user, priv,
                             "needed=%u, done=%u", priv->recv.needed, priv->recv.done);
            abort();
        }

        if (priv->recv.current.opcode == CWS_OPCODE_CONTINUATION) {
            if (priv->recv.fragmented.opcode == 0)
                cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, "nothing to continue", SIZE_MAX);
            if (priv->recv.current.payload)
                free(priv->recv.current.payload);

            priv->recv.current.payload = priv->recv.fragmented.payload;
            priv->recv.current.used = priv->recv.fragmented.used;
            priv->recv.current.total = priv->recv.fragmented.total;
            priv->recv.fragmented.payload = NULL;
            priv->recv.fragmented.used = 0;
            priv->recv.fragmented.total = 0;
        } else if (!_cws_opcode_is_control(priv->recv.current.opcode) && priv->recv.fragmented.opcode != 0) {
            cws_close(priv, CWS_CLOSE_REASON_PROTOCOL_ERROR, "expected continuation or control frames", SIZE_MAX);
        }

        if (frame_len > 0) {
            void *tmp;

            tmp = realloc(priv->recv.current.payload,
                          priv->recv.current.total + frame_len + 1);
            if (!tmp) {
                cws_close(priv, CWS_CLOSE_REASON_TOO_BIG, NULL, 0);
                (priv->debug_fn)(priv->user, priv, "could not allocate memory");
                return CURL_READFUNC_ABORT;
            }
            priv->recv.current.payload = tmp;
            priv->recv.current.total += frame_len;
        }
    }

    if (len == 0 && priv->recv.done < priv->recv.needed)
        return used;

    /* fill payload */
    while (len > 0 && priv->recv.current.used < priv->recv.current.total) {
        size_t todo = priv->recv.current.total - priv->recv.current.used;
        if (todo > len)
            todo = len;
        memcpy(priv->recv.current.payload + priv->recv.current.used, buffer, todo);
        priv->recv.current.used += todo;
        used += todo;
        buffer += todo;
        len -= todo;
    }

    if (priv->recv.current.payload)
        priv->recv.current.payload[priv->recv.current.used] = '\0';

    if (len == 0 && priv->recv.current.used < priv->recv.current.total)
        return used;

    priv->dispatching++;

    _cws_dispatch(priv);

    priv->recv.done = 0;
    priv->recv.needed = sizeof(struct cws_frame_header);
    priv->recv.current.used = 0;
    priv->recv.current.total = 0;

    priv->dispatching--;
    _cws_cleanup(priv);

    return used;
}

static size_t _cws_receive_data(const char *buffer, size_t count, size_t nitems, void *data)
{
    CWS *priv = data;
    size_t len = count * nitems;
    while (len > 0) {
        size_t used = _cws_process_frame(priv, buffer, len);
        len -= used;
        buffer += used;
    }

    return count * nitems;
}

static size_t _cws_send_data(char *buffer, size_t count, size_t nitems, void *data)
{
    CWS *priv = data;
    size_t len = count * nitems;
    size_t todo = priv->send.len;

    if (todo == 0) {
        priv->pause_flags |= CURLPAUSE_SEND;
        return CURL_READFUNC_PAUSE;
    }
    if (todo > len)
        todo = len;

    memcpy(buffer, priv->send.buffer, todo);
    if (todo < priv->send.len) {
        /* optimization note: we could avoid memmove() by keeping a
         * priv->send.position, then we just increment that offset.
         *
         * on next _cws_write(), check if priv->send.position > 0 and
         * memmove() to make some space without realloc().
         */
        memmove(priv->send.buffer,
                priv->send.buffer + todo,
                priv->send.len - todo);
    } else {
        free(priv->send.buffer);
        priv->send.buffer = NULL;
    }

    priv->send.len -= todo;
    return todo;
}


/*
 * @returnval CWSE_OK
 * @returnval CWSE_OUT_OF_MEMORY
 */
static CWScode _cws_write(CWS *priv, const void *buffer, size_t len)
{
    /* optimization note: we could grow by some rounded amount (ie:
     * next power-of-2, 4096/pagesize...) and if using
     * priv->send.position, do the memmove() here to free up some
     * extra space without realloc() (see _cws_send_data()).
     */
    //_cws_debug("WRITE", buffer, len);
    uint8_t *tmp = realloc(priv->send.buffer, priv->send.len + len);
    if (!tmp)
        return CWSE_OUT_OF_MEMORY;
    memcpy(tmp + priv->send.len, buffer, len);
    priv->send.buffer = tmp;
    priv->send.len += len;
    if (priv->pause_flags & CURLPAUSE_SEND) {
        priv->pause_flags &= ~CURLPAUSE_SEND;
        curl_easy_pause(priv->easy, priv->pause_flags);
    }
    return CWSE_OK;
}

/*
 * Mask is:
 *
 *     for i in len:
 *         output[i] = input[i] ^ mask[i % 4]
 *
 * Here a temporary buffer is used to reduce number of "write" calls
 * and pointer arithmetic to avoid counters.
 *
 * @returnval CWSE_OK
 * @returnval CWSE_OUT_OF_MEMORY
 */
static CWScode _cws_write_masked(CWS *priv, const uint8_t *mask, const void *buffer, size_t len)
{
    const uint8_t *itr_begin = buffer;
    const uint8_t *itr = itr_begin;
    const uint8_t *itr_end = itr + len;
    uint8_t tmpbuf[CWS_MASK_TMPBUF_SIZE];
    CWScode rv = CWSE_OK;

    while (itr < itr_end) {
        uint8_t *o = tmpbuf, *o_end = tmpbuf + sizeof(tmpbuf);
        for (; o < o_end && itr < itr_end; o++, itr++) {
            *o = *itr ^ mask[(itr - itr_begin) & 0x3];
        }
        rv = _cws_write(priv, tmpbuf, o - tmpbuf);
        if (CWSE_OK != rv) {
            return rv;
        }
    }

    return rv;
}

/*
 * @returnval CWSE_OK
 * @returnval CWSE_OUT_OF_MEMORY
 * @returnval CWSE_CLOSED_CONNECTION
 */
static CWScode _cws_send(CWS *priv, enum cws_opcode opcode, const void *msg, size_t msglen)
{
    CWScode rv = CWSE_OK;
    uint8_t buffer[14]; /* 2 (base) + 8 (uint64_t) + 4 (mask) */
    uint8_t *mask;
    size_t header_len;

    if (priv->closed) {
        (priv->debug_fn)(priv->user, priv,
                         "cannot send data to closed WebSocket connection %p",
                         priv->easy);
        return CWSE_CLOSED_CONNECTION;
    }

    memset(buffer, 0, sizeof(buffer));

    buffer[0] = 0x80 | (0x0f & opcode);

    header_len = 2;
    if (msglen <= 125) {
        buffer[1] = 0x80 | (uint8_t) (0x7f & msglen);
        mask = &buffer[2];
    } else if (msglen <= UINT16_MAX) {
        buffer[1] = 0x80 | 126;
        buffer[2] = (uint8_t) (0x00ff & (msglen >> 8));
        buffer[3] = (uint8_t) (0x00ff & msglen);
        mask = &buffer[4];
        header_len = 4;
    } else {
        buffer[1] = 0x80 | 127;
        if (8 == sizeof(size_t)) {
            buffer[2] = (uint8_t) (0x00ff & (msglen >> 56));
            buffer[3] = (uint8_t) (0x00ff & (msglen >> 48));
            buffer[4] = (uint8_t) (0x00ff & (msglen >> 40));
            buffer[5] = (uint8_t) (0x00ff & (msglen >> 32));
        }

        if (2 < sizeof(size_t)) {
            buffer[6] = (uint8_t) (0x00ff & (msglen >> 24));
            buffer[7] = (uint8_t) (0x00ff & (msglen >> 16));
        }

        buffer[8] = (uint8_t) (0x00ff & (msglen >>  8));
        buffer[9] = (uint8_t) (0x00ff & msglen);
        mask = &buffer[10];
        header_len = 10;
    }


    priv->get_random_fn(priv->user, priv, mask, 4);
    header_len += 4;

    rv = _cws_write(priv, buffer, header_len);
    if (CWSE_OK != rv) {
        return rv;
    }

    return _cws_write_masked(priv, mask, msg, msglen);
}


static void _cws_cleanup(CWS *priv)
{
    if (priv->dispatching > 0)
        return;

    if (!priv->deleted)
        return;

    if (priv) {
        if (priv->headers) {
            curl_slist_free_all(priv->headers);
        }
        if (priv->url) {
            free(priv->url);
        }
        if (priv->easy) {
            curl_easy_cleanup(priv->easy);
            priv->easy = NULL;
        }
        if (priv->websocket_protocols.requested) {
            free(priv->websocket_protocols.requested);
        }
        if (priv->websocket_protocols.received) {
            free(priv->websocket_protocols.received);
        }
        if (priv->expected_key_header) {
            free(priv->expected_key_header);
        }
        if (priv->recv.current.payload) {
            free(priv->recv.current.payload);
        }
        if (priv->recv.fragmented.payload) {
            free(priv->recv.fragmented.payload);
        }
        if (priv->send.buffer) {
            free(priv->send.buffer);
        }
        free(priv);
    }
}

/*----------------------------------------------------------------------------*/
/*                      Internal Configuration Functions                      */
/*----------------------------------------------------------------------------*/
static bool _validate_config(const struct cws_config *config)
{
    struct curl_slist *p;
    const char *disallowed[] = {
        "Connection",
        "Content-Length",
        "Content-Type",
        "Expect",
        "Sec-WebSocket-Accept",
        "Sec-WebSocket-Key",
        "Sec-WebSocket-Protocol",
        "Sec-WebSocket-Version",
        "Transfer-Encoding",
        "Upgrade"
    };

    if ((NULL == config) || (NULL == config->url)) {
        return false;
    }

    if (config->max_redirects < -1) {
        return false;
    }

    p = config->extra_headers;
    while (p) {
        size_t i;
        for (i = 0; i < sizeof(disallowed) / sizeof(disallowed[0]); i++) {
            if (cws_has_prefix(p->data, strlen(p->data), disallowed[i])) {
                return false;
            }
        }
        p = p->next;
    }

    switch (config->ip_version) {
        case 0:
        case 4:
        case 6:
            break;
        default:
            return false;
    }

    switch (config->verbose) {
        case 0:
        case 1:
        case 2:
        case 3:
            break;
        default:
            return false;
    }

    switch (config->explicit_expect) {
        case 0:
        case 1:
            break;
        default:
            return false;
    }

    return true;
}

static bool _cws_calculate_websocket_key(CWS *priv)
{
    const char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const char header[] = "Sec-WebSocket-Key: ";

    uint8_t sha1_value[20];
    uint8_t random_value[16];
    char b64_key[25];               /* b64(16 bytes) = 24 bytes + 1 for '\0' */
    char expected[29];              /* b64(20 bytes) = 28 bytes + 1 for '\0' */
 
    /* For the next two values: -1 because only 1 trailing '\0' is needed */
    char combined[sizeof(b64_key) + sizeof(guid) - 1];
    char send[sizeof(header) + sizeof(b64_key) - 1];

    if (!priv->expected_key_header) {
        struct curl_slist *tmp;

        (priv->get_random_fn)(priv->user, priv, random_value, sizeof(random_value));
        cws_encode_base64(random_value, sizeof(random_value), b64_key);

        snprintf(send, sizeof(send), "%s%s", header, b64_key);

        tmp = curl_slist_append(priv->headers, send);
        if (tmp) {
            priv->headers = tmp;

            snprintf(combined, sizeof(combined), "%s%s", b64_key, guid);

            cws_sha1(combined, strlen(combined), sha1_value);
            cws_encode_base64(sha1_value, sizeof(sha1_value), expected);

            priv->expected_key_header = cws_strdup(expected);

            if (priv->expected_key_header) {
                return true;
            }
        }
    }
    return false;
}

static bool _cws_add_websocket_protocols(CWS *priv, const char *websocket_protocols)
{
    char *buf;
    struct curl_slist *p;

    if (!websocket_protocols) {
        return true;
    }

    buf = cws_strmerge("Sec-WebSocket-Protocol: ", websocket_protocols);
    if (!buf) {
        return false;
    }

    p = curl_slist_append(priv->headers, buf);
    free(buf);

    if (p) {
        priv->headers = p;
        priv->websocket_protocols.requested = cws_strdup(websocket_protocols);
        if (priv->websocket_protocols.requested) {
            return true;
        }
    }

    return false;
}

static bool _cws_add_extra_headers(CWS *priv, const struct cws_config *config)
{
    struct curl_slist *p, *tmp;

    p = config->extra_headers;
    while (p) {
        tmp = curl_slist_append(priv->headers, p->data);
        if (!tmp) {
            return false;
        }

        priv->headers = tmp;
        p = p->next;
    }

    return true;
}

static void _populate_callbacks(CWS *dest, const struct cws_config *src)
{
    dest->on_connect_fn = _default_on_connect;
    dest->on_text_fn    = _default_on_text;
    dest->on_binary_fn  = _default_on_binary;
    dest->on_ping_fn    = _default_on_ping;
    dest->on_pong_fn    = _default_on_pong;
    dest->on_close_fn   = _default_on_close;
    dest->get_random_fn = _default_get_random;
    dest->debug_fn      = _default_debug;
    dest->configure_fn  = _default_configure;

    if (NULL == src) {
        return;
    }

    /* Optionally select the verbose debug handler */
    if (0 != (1 & src->verbose)) {
        dest->debug_fn = _default_verbose_debug;
    }

    /* Overwrite the default values with the user specified values. */
    if (src->on_connect) {
        dest->on_connect_fn = src->on_connect;
    }
    if (src->on_text) {
        dest->on_text_fn = src->on_text;
    }
    if (src->on_binary) {
        dest->on_binary_fn = src->on_binary;
    }
    if (src->on_ping) {
        dest->on_ping_fn = src->on_ping;
    }
    if (src->on_pong) {
        dest->on_pong_fn = src->on_pong;
    }
    if (src->on_close) {
        dest->on_close_fn = src->on_close;
    }
    if (src->get_random) {
        dest->get_random_fn = src->get_random;
    }
    if (src->debug) {
        dest->debug_fn = src->debug;
    }
    if (src->configure) {
        dest->configure_fn = src->configure;
    }
}

static void _default_on_connect(void *data, CWS *priv, const char *websocket_protocols)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(websocket_protocols);
}


static void _default_on_text(void *data, CWS *priv, const char *text, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(text);
    IGNORE_UNUSED(len);
}


static void _default_on_binary(void *data, CWS *priv, const void *mem, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(mem);
    IGNORE_UNUSED(len);
}


static void _default_on_ping(void *data, CWS *priv, const void *reason, size_t len)
{
    IGNORE_UNUSED(data);

    cws_pong(priv, reason, len);
}


static void _default_on_pong(void *data, CWS *priv, const void *reason, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(reason);
    IGNORE_UNUSED(len);
}


static void _default_on_close(void *data, CWS *priv, int status, const char *reason, size_t len)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(status);
    IGNORE_UNUSED(reason);
    IGNORE_UNUSED(len);
}


static void _default_get_random(void *data, CWS *priv, void *buffer, size_t len)
{
    uint8_t *bytes = buffer;
    size_t i;

    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);

    for (i = 0; i < len; i++) {
        bytes[i] = (0x0ff & rand());
    }
}


static void _default_configure(void *data, CWS *priv, CURL *easy)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(easy);
}


static void _default_debug(void *data, CWS *priv, const char *format, ...)
{
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(format);
}


static void _default_verbose_debug(void *data, CWS *priv, const char *format, ...)
{
    va_list args;

    IGNORE_UNUSED(data);
    IGNORE_UNUSED(priv);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

