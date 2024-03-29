/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <curlws/curlws.h>
#include <trower-base64/base64.h>

#include "data_block_sender.h"
#include "frame_senders.h"
#include "handlers.h"
#include "header.h"
#include "internal.h"
#include "memory.h"
#include "random.h"
#include "receive.h"
#include "send.h"
#include "sha1.h"
#include "utf8.h"
#include "utils.h"
#include "verbose.h"
#include "ws.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define CURLWS_MIN_VERSION        0x073202
#define CURLWS_MIN_VERSION_STRING "7.50.2"

#if !CURL_AT_LEAST_VERSION(0x07, 0x32, 0x02)
#error "curl minimum version not met 7.50.2"
#endif

/* Before curl 7.54.0 the MAX_DEFAULT was missing, so choose the best
 * available TLS */
#if !CURL_AT_LEAST_VERSION(0x07, 0x36, 0x00)
/* If before curl 7.54.0 then the best TLS available is 1.3 */
#define CURL_SSLVERSION_MAX_DEFAULT CURL_SSLVERSION_TLSv1_3
#elif !CURL_AT_LEAST_VERSION(0x07, 0x34, 0x00)
/* If before curl 7.52.0 then the best TLS available is 1.2 */
#define CURL_SSLVERSION_MAX_DEFAULT CURL_SSLVERSION_TLSv1_2
#endif

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
static CWScode _normalize_close_inputs(int *, int *, const char **, size_t *);
static int _check_curl_version(const struct cws_config *);
CWScode _send_stream(CWS *, int, int, const void *, size_t);
static CURLcode _config_url(CWS *, const struct cws_config *);
static CURLcode _config_redirects(CWS *, const struct cws_config *);
static CURLcode _config_security(CWS *, const struct cws_config *);
static CURLcode _config_verbosity(CWS *, const struct cws_config *);
static CURLcode _config_ws_workarounds(CWS *, const struct cws_config *);
static CURLcode _config_memorypool(CWS *, const struct cws_config *);
static CURLcode _config_ws_key(CWS *);
static CURLcode _config_ws_protocols(CWS *, const struct cws_config *);
static CURLcode _config_http_headers(CWS *, const struct cws_config *);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

CWS *cws_create(const struct cws_config *config)
{
    CURLcode status = CURLE_OK;
    CWS *priv       = NULL;

    if (!config) {
        return NULL;
    }

    if (0 != _check_curl_version(config)) {
        return NULL;
    }

    priv = (CWS *) calloc(1, sizeof(struct cws_object));
    if (!priv) {
        return NULL;
    }

    priv->easy = curl_easy_init();
    if (!priv->easy) {
        cws_destroy(priv);
        return NULL;
    }

    priv->cfg.user = config->user;

    populate_callbacks(&priv->cb, config);
    status |= _config_memorypool(priv, config);
    status |= _config_url(priv, config);
    status |= _config_security(priv, config);
    status |= _config_redirects(priv, config);
    status |= header_init(priv);
    status |= receive_init(priv);
    status |= send_init(priv);
    status |= _config_verbosity(priv, config);
    status |= _config_ws_workarounds(priv, config);
    status |= _config_ws_key(priv);
    status |= _config_ws_protocols(priv, config);
    status |= _config_http_headers(priv, config);

    /* You can overwrite anything you want, but be very careful! */
    if (config->configure) {
        status |= (*config->configure)(priv->cfg.user, priv, priv->easy);
    }

    if (CURLE_OK != status) {
        cws_destroy(priv);
        return NULL;
    }

    return priv;
}


void cws_destroy(CWS *priv)
{
    if (priv) {
        if (priv->dispatching > 0)
            return;

        if (priv->cfg.url) {
            free(priv->cfg.url);
        }
        if (priv->easy) {
            curl_easy_cleanup(priv->easy);
        }
        if (priv->headers) {
            curl_slist_free_all(priv->headers);
        }
        if (priv->cfg.ws_protocols_requested) {
            free(priv->cfg.ws_protocols_requested);
        }
        if (priv->header_state.ws_protocols_received) {
            free(priv->header_state.ws_protocols_received);
        }
        if (priv->expected_key_header) {
            free(priv->expected_key_header);
        }

        send_destroy(priv);

        if (priv->mem) {
            mem_cleanup_pool(priv->mem);
        }
        if (priv->stream_buffer) {
            free(priv->stream_buffer);
        }

        free(priv);
    }
}


CURLMcode cws_multi_add_handle(CWS *priv, CURLM *multi_handle)
{
    if (!priv) {
        /* CURLM_BAD_FUNCTION_ARGUMENT is only available since 7.69.0
         * so use a much older error code. */
        return CURLM_UNKNOWN_OPTION;
    }

    return curl_multi_add_handle(multi_handle, priv->easy);
}


CURLMcode cws_multi_remove_handle(CWS *priv, CURLM *multi_handle)
{
    if (!priv) {
        /* CURLM_BAD_FUNCTION_ARGUMENT is only available since 7.69.0
         * so use a much older error code. */
        return CURLM_UNKNOWN_OPTION;
    }

    return curl_multi_remove_handle(multi_handle, priv->easy);
}


CWScode cws_ping(CWS *priv, const void *data, size_t len)
{
    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    return frame_sender_control(priv, CWS_PING, data, len);
}


CWScode cws_pong(CWS *priv, const void *data, size_t len)
{
    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    return frame_sender_control(priv, CWS_PONG, data, len);
}


CWScode cws_close(CWS *priv, int code, const char *reason, size_t len)
{
    int options;
    CWScode rv;
    uint8_t buf[WS_CTL_PAYLOAD_MAX]; /* Limited by RFC6455 Section 5.5 */
    uint8_t *p = NULL;

    if (!priv || (!reason && (0 < len))) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    rv = _normalize_close_inputs(&code, &options, &reason, &len);
    if (CWSE_OK != rv) {
        return rv;
    }

    if (code) {
        p    = buf;
        *p++ = (uint8_t) (0x00ff & (code >> 8));
        *p++ = (uint8_t) (0x00ff & code);

        if (len) {
            memcpy(p, reason, len);
        }
        len += 2;
        p = buf;
    }

    rv = frame_sender_control(priv, options, p, len);
    if (!(CLOSE_QUEUED & priv->close_state)) {
        priv->close_state |= CLOSE_QUEUED;
        verbose_close(priv);
    }
    return rv;
}


CWScode cws_send_blk_binary(CWS *priv, const void *data, size_t len)
{
    if (!priv || (!data && (0 < len))) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    return data_block_sender(priv, CWS_BINARY, data, len);
}


CWScode cws_send_blk_text(CWS *priv, const char *s, size_t len)
{
    if (!priv || (!s && (0 < len))) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    if (s && (0 < len)) {
        size_t prev_len;

        if (SIZE_MAX == len) {
            len = strlen(s);
        }

        prev_len = len;
        if (0 != utf8_validate(s, &len) || (prev_len != len)) {
            return CWSE_INVALID_UTF8;
        }
    }

    return data_block_sender(priv, CWS_TEXT, s, len);
}


CWScode cws_send_strm_binary(CWS *priv, int info, const void *data, size_t len)
{
    return _send_stream(priv, CWS_BINARY, info, data, len);
}


CWScode cws_send_strm_text(CWS *priv, int info, const char *s, size_t len)
{
    if (s && (0 < len)) {
        if (SIZE_MAX == len) {
            len = strlen(s);
        }

        if (0 != utf8_validate(s, &len)) {
            return CWSE_INVALID_UTF8;
        }
    }


    return _send_stream(priv, CWS_TEXT, info, s, len);
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static CWScode _normalize_close_inputs(int *_code, int *_opts,
                                       const char **_reason, size_t *_len)
{
    int opts           = CWS_CLOSE;
    int code           = *_code;
    size_t len         = *_len;
    const char *reason = *_reason;

    /* Handle the urgent version */
    if (code < 0) {
        opts |= CWS_URGENT;

        if (-1 == code) {
            code = 0;
        } else {
            code = 0 - code; /* Convert to positive */
        }
    }

    /* Normalize the reason and len */
    if (reason) {
        if (len == SIZE_MAX) {
            len = strlen(reason);
        }

        if (0 < len) {
            size_t prev_len = len;
            if (0 != utf8_validate(reason, &len) || (prev_len != len)) {
                return CWSE_INVALID_UTF8;
            }

            /* 2 byte reason code + '\0' at the end. */
            if ((WS_CTL_PAYLOAD_MAX - 3) < len) {
                return CWSE_APP_DATA_LENGTH_TOO_LONG;
            }
        }
    }

    /* Handle the special 0 code case */
    if (0 == code) {
        /* You can't have a code of 0 and expect to send reason text. */
        if (0 < len) {
            return CWSE_INVALID_CLOSE_REASON_CODE;
        }
    } else {
        if (false == is_close_code_valid(code)) {
            return CWSE_INVALID_CLOSE_REASON_CODE;
        }
    }

    /* Transfer the results back only if successful */
    *_code   = code;
    *_len    = len;
    *_opts   = opts;
    *_reason = reason;

    return CWSE_OK;
}


static int _check_curl_version(const struct cws_config *config)
{
    const curl_version_info_data *curl_ver = NULL;
    curl_ver                               = curl_version_info(CURLVERSION_NOW);

    if (curl_ver->version_num < CURLWS_MIN_VERSION) {
        if (config->verbose) {
            FILE *err = stderr;

            if (NULL != config->verbose_stream) {
                err = config->verbose_stream;
            }

            fprintf(err,
                    "ERROR: CURL version '%s'. At least '%s' is required "
                    "for curlws to work reliably\n",
                    curl_ver->version,
                    CURLWS_MIN_VERSION_STRING);
        }
        return -1;
    }
    return 0;
}


CWScode _send_stream(CWS *priv, int type, int info, const void *data, size_t len)
{
    if (!priv || (!data && (0 < len))) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    /* If the message has no payload and is not a start or end block, don't
     * send it. */
    if ((0 == info) && (0 == len)) {
        if (0 != priv->last_sent_data_frame_info) {
            return CWSE_OK;
        } else {
            return CWSE_STREAM_CONTINUITY_ISSUE;
        }
    }

    if (CWS_FIRST & info) {
        info |= type;
    } else {
        info |= CWS_CONT;
    }

    return frame_sender_data(priv, info, data, len);
}


/*----------------------------------------------------------------------------*/
/*                      Internal Configuration Functions                      */
/*----------------------------------------------------------------------------*/
static CURLcode _config_url(CWS *priv, const struct cws_config *config)
{
    if (config->url) {
        priv->cfg.url = cws_rewrite_url(config->url);
        if (priv->cfg.url) {
            return curl_easy_setopt(priv->easy, CURLOPT_URL, priv->cfg.url);
        }
    }

    return ~CURLE_OK;
}


static CURLcode _config_redirects(CWS *priv, const struct cws_config *config)
{
    CURLcode rv = CURLE_OK;

    /* Invalid, too low */
    if (config->max_redirects < -1) {
        return ~CURLE_OK;
    }

    /* Setup redirect limits. */
    if (0 != config->max_redirects) {
        rv |= curl_easy_setopt(priv->easy, CURLOPT_FOLLOWLOCATION, 1L);
        rv |= curl_easy_setopt(priv->easy, CURLOPT_MAXREDIRS, config->max_redirects);

        priv->cfg.follow_redirects = true;
    }

    return rv;
}


static CURLcode _config_security(CWS *priv, const struct cws_config *config)
{
    (void) config;

    /* Force a reasonably modern version of TLS */
    return curl_easy_setopt(priv->easy, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
}


static CURLcode _config_verbosity(CWS *priv, const struct cws_config *config)
{
    CURLcode rv = CURLE_OK;

    if (config->verbose < 0) {
        return ~CURLE_OK;
    }

    priv->cfg.verbose_stream = stderr;
    if (config->verbose_stream) {
        priv->cfg.verbose_stream = config->verbose_stream;
    }

    priv->cfg.verbose = config->verbose;
    if (1 < config->verbose) {
        rv |= curl_easy_setopt(priv->easy, CURLOPT_VERBOSE, 1L);
        if (config->verbose_stream) {
            rv |= curl_easy_setopt(priv->easy, CURLOPT_STDERR, priv->cfg.verbose_stream);
        }
    }

    if (3 < priv->cfg.verbose) {
        priv->cfg.verbose = 3;
    }

    return rv;
}


static CURLcode _config_ws_workarounds(CWS *priv, const struct cws_config *config)
{
    struct curl_slist *p;
    CURLcode rv = CURLE_OK;

    /*
     * BEGIN: work around CURL to get WebSocket:
     *
     * WebSocket must be HTTP/1.1 GET request where we must keep the
     * "send" part alive without any content-length and no chunked
     * encoding and the server answer is 101-upgrade.
     */
    rv |= curl_easy_setopt(priv->easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    /* Use CURLOPT_UPLOAD=1 to force "send" even with a GET request,
     * however it will set HTTP request to PUT
     */
    rv |= curl_easy_setopt(priv->easy, CURLOPT_UPLOAD, 1L);

    /*
     * Then we manually override the string sent to be "GET".
     */
    rv |= curl_easy_setopt(priv->easy, CURLOPT_CUSTOMREQUEST, "GET");

    /* We need to close connections and not allow reuse. */
    rv |= curl_easy_setopt(priv->easy, CURLOPT_FORBID_REUSE, 1L);

    /* We need to freshly connect each time. */
    rv |= curl_easy_setopt(priv->easy, CURLOPT_FRESH_CONNECT, 1L);

    /*
     * CURLOPT_UPLOAD=1 with HTTP/1.1 implies:
     *     Expect: 100-continue
     * but we don't want that, rather 101. Then force: 101.
     */
    if (1 == config->expect) {
        p = curl_slist_append(priv->headers, "Expect: 101");
        if (!p) {
            return ~CURLE_OK;
        }
        priv->headers = p;
    } else if (0 != config->expect) {
        return ~CURLE_OK;
    }

    /*
     * CURLOPT_UPLOAD=1 without a size implies in:
     *     Transfer-Encoding: chunked
     * but we don't want that, rather unmodified (raw) bites as we're
     * doing the websockets framing ourselves. Force nothing.
     */
    p = curl_slist_append(priv->headers, "Transfer-Encoding:");
    if (!p) {
        return ~CURLE_OK;
    }
    priv->headers = p;

    /* END: work around CURL to get WebSocket. */

    return rv;
}


static CURLcode _config_memorypool(CWS *priv, const struct cws_config *config)
{
    size_t max_payload_size;

    priv->cfg.max_payload_size = 1024;
    if (config->max_payload_size) {
        priv->cfg.max_payload_size = config->max_payload_size;
    }

    /* The largest frame size, so allocate this amount. */
    max_payload_size = priv->cfg.max_payload_size + WS_FRAME_HEADER_MAX;

    priv->mem_cfg.data_block_size    = send_get_memory_needed(max_payload_size);
    priv->mem_cfg.control_block_size = send_get_memory_needed(WS_CTL_FRAME_MAX);

    priv->mem = mem_init_pool(&priv->mem_cfg);

    return CURLE_OK;
}


static CURLcode _config_ws_key(CWS *priv)
{
    CURLcode rv         = ~CURLE_OK;
    const char guid[]   = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const char header[] = "Sec-WebSocket-Key: ";

    uint8_t sha1_md[20];
    uint8_t random_value[16];
    struct curl_slist *tmp = NULL;
    char *b64_key          = NULL;
    char *combined         = NULL;
    char *send             = NULL;

    cws_random(priv, random_value, sizeof(random_value));

    b64_key = b64_encode_with_alloc(random_value, sizeof(random_value), NULL);
    if (!b64_key) {
        return rv;
    }

    send     = cws_strmerge(header, b64_key);
    combined = cws_strmerge(b64_key, guid);
    if (!send || !combined) {
        goto err;
    }

    if (0 != cws_sha1(combined, strlen(combined), sha1_md)) {
        goto err;
    }

    tmp = curl_slist_append(priv->headers, send);
    if (!tmp) {
        goto err;
    }
    priv->headers = tmp;

    priv->expected_key_header = b64_encode_with_alloc(sha1_md, sizeof(sha1_md),
                                                      &priv->expected_key_header_len);

    rv = CURLE_OK;

err:
    if (b64_key) {
        free(b64_key);
    }
    if (combined) {
        free(combined);
    }
    if (send) {
        free(send);
    }

    return rv;
}

static CURLcode _config_ws_protocols(CWS *priv, const struct cws_config *config)
{
    struct curl_slist *tmp;
    char *buf;

    if (!config->websocket_protocols) {
        return CURLE_OK;
    }

    buf = cws_strmerge("Sec-WebSocket-Protocol: ", config->websocket_protocols);
    if (!buf) {
        return ~CURLE_OK;
    }

    tmp = curl_slist_append(priv->headers, buf);
    free(buf);

    if (!tmp) {
        return ~CURLE_OK;
    }

    priv->headers = tmp;

    priv->cfg.ws_protocols_requested = cws_strdup(config->websocket_protocols);
    if (!priv->cfg.ws_protocols_requested) {
        return ~CURLE_OK;
    }

    return CURLE_OK;
}


static CURLcode _config_http_headers(CWS *priv, const struct cws_config *config)
{
    const char *disallowed[] = {
        "Connection:",
        "Content-Length:",
        "Content-Type:",
        "Expect:",
        "Sec-WebSocket-Accept:",
        "Sec-WebSocket-Key:",
        "Sec-WebSocket-Protocol:",
        "Sec-WebSocket-Version:",
        "Transfer-Encoding:",
        "Upgrade:"
    };
    const char *ws_headers[] = {
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13",
    };
    CURLcode rv              = CURLE_OK;
    struct curl_slist *p     = NULL;
    struct curl_slist *extra = NULL;

    /* Add the required headers */
    for (size_t i = 0; i < sizeof(ws_headers) / sizeof(ws_headers[0]); i++) {
        p = curl_slist_append(priv->headers, ws_headers[i]);
        if (!p) {
            return ~CURLE_OK;
        }
        priv->headers = p;
    }


    /* Add the extra headers */
    extra = config->extra_headers;
    while (extra) {
        /* Validate the extra header is ok */
        for (size_t i = 0; i < sizeof(disallowed) / sizeof(disallowed[0]); i++) {
            if (cws_has_prefix(extra->data, strlen(extra->data), disallowed[i])) {
                return ~CURLE_OK;
            }
        }

        p = curl_slist_append(priv->headers, extra->data);
        if (!p) {
            return ~CURLE_OK;
        }
        priv->headers = p;
        extra         = extra->next;
    }

    rv |= curl_easy_setopt(priv->easy, CURLOPT_HTTPHEADER, priv->headers);

    return rv;
}
