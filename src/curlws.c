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
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "curlws.h"
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
#include "utils.h"
#include "utf8.h"
#include "ws.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define CURLWS_MIN_VERSION          0x073202
#define CURLWS_MIN_VERSION_STRING   "7.50.2"

#if !CURL_AT_LEAST_VERSION(0x07, 0x32, 0x02)
#error "curl minimum version not met 7.50.2"
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
static CWScode _normalize_close_inputs(int*, int*, const char**, size_t*);
static int _check_curl_version(void);
static int _config_url(CWS*, const struct cws_config*);
static int _config_redirects(CWS*, const struct cws_config*);
static int _config_ip_version(CWS*, const struct cws_config*);
static int _config_interface(CWS*, const struct cws_config*);
static int _config_security(CWS*, const struct cws_config*);
static int _config_verbosity(CWS*, const struct cws_config*);
static int _config_ws_workarounds(CWS*, const struct cws_config*);
static int _config_memorypool(CWS*, const struct cws_config*);
static int _config_ws_key(CWS*);
static int _config_ws_protocols(CWS*, const struct cws_config*);
static int _config_http_headers(CWS*, const struct cws_config*);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

CWS *cws_create(const struct cws_config *config)
{
    int error = 0;
    CWS *priv = NULL;

    if ((0 != _check_curl_version()) || (NULL == config)) {
        return NULL;
    }

    priv = (CWS*) calloc(1, sizeof(struct cws_object));
    if (!priv) {
        return NULL;
    }

    priv->easy = curl_easy_init();
    if (!priv->easy) {
        cws_destroy(priv);
        return NULL;
    }

    priv->cfg.user = config->user;

    /* Only place things here that are "ok" for a user to overwrite. */

    if (config->configure) {
        (*config->configure)(priv->cfg.user, priv, priv->easy);
    }

    populate_callbacks(&priv->cb, config);
    error |= _config_memorypool(priv, config);
    error |= _config_url(priv, config);
    error |= _config_security(priv, config);
    error |= _config_interface(priv, config);
    error |= _config_redirects(priv, config);
    error |= _config_ip_version(priv, config);

    header_init(priv);
    receive_init(priv);
    send_init(priv);

    error |= _config_verbosity(priv, config);
    error |= _config_ws_workarounds(priv, config);
    error |= _config_ws_key(priv);
    error |= _config_ws_protocols(priv, config);
    error |= _config_http_headers(priv, config);

    if (error) {
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


CURLMcode cws_multi_add_handle(CWS *ws, CURLM *multi_handle)
{
    return curl_multi_add_handle(multi_handle, ws->easy);
}


CURLMcode cws_multi_remove_handle(CWS *ws, CURLM *multi_handle)
{
    return curl_multi_remove_handle(multi_handle, ws->easy);
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
    uint8_t buf[WS_CTL_PAYLOAD_MAX];    /* Limited by RFC6455 Section 5.5 */
    uint8_t *p = NULL;

    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    rv = _normalize_close_inputs(&code, &options, &reason, &len);
    if (CWSE_OK != rv) {
        return rv;
    }

    if (code) {
        p = buf;
        *p++ = (uint8_t) (0x00ff & (code >> 8));
        *p++ = (uint8_t) (0x00ff & code);
        
        if (len) {
            memcpy(p, reason, len);
            p[len] = '\0';
            len++;
        }
        len += 2;
        p = buf;
    }

    rv = frame_sender_control(priv, options, p, len);
    priv->closed = true;
    return rv;
}


CWScode cws_send_blk_binary(CWS *priv, const void *data, size_t len)
{
    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    return data_block_sender(priv, CWS_BINARY, data, len);
}


CWScode cws_send_blk_text(CWS *priv, const char *s, size_t len)
{
    size_t prev_len;

    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    if (s) {
        if (len == SIZE_MAX) {
            len = strlen(s);
        }
    } else {
        len = 0;
    }

    prev_len = len;
    if (0 != utf8_validate(s, &len) || (prev_len != len)) {
        return CWSE_INVALID_UTF8;
    }

    return data_block_sender(priv, CWS_TEXT, s, len);
}


CWScode cws_send_strm_binary(CWS *priv, int info, const void *data, size_t len)
{

    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    /* If the message has no payload and is not a start or end block, don't
     * send it. */
    if ((0 == info) && ((!data) || (0 == len))) {
        if (0 != priv->last_sent_data_frame_info) {
            return CWSE_OK;
        } else {
            return CWSE_STREAM_CONTINUITY_ISSUE;
        }
    }

    if (CWS_FIRST & info) {
        info |= CWS_BINARY;
    } else {
        info |= CWS_CONT;
    }

    return frame_sender_data(priv, info, data, len);
}


CWScode cws_send_strm_text(CWS *priv, int info, const char *s, size_t len)
{
    if (!priv) {
        return CWSE_BAD_FUNCTION_ARGUMENT;
    }

    /* If the message has no payload and is not a start or end block, don't
     * send it. */
    if ((0 == info) && ((!s) || (0 == len))) {
        if (0 != priv->last_sent_data_frame_info) {
            return CWSE_OK;
        } else {
            return CWSE_STREAM_CONTINUITY_ISSUE;
        }
    }

    if (CWS_FIRST & info) {
        info |= CWS_TEXT;
    } else {
        info |= CWS_CONT;
    }

    if (0 != utf8_validate(s, &len)) {
        return CWSE_INVALID_UTF8;
    }

    return frame_sender_data(priv, info, s, len);
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static CWScode _normalize_close_inputs(int *_code, int *_opts,
                                       const char **_reason, size_t *_len)
{
    int opts = CWS_CLOSE;
    int code = *_code;
    size_t len = *_len;
    const char *reason = *_reason;

    /* Handle the urgent version */
    if (code < 0) {
        opts |= CWS_URGENT;

        if (-1 == code) {
            code = 0;
        } else {
            code = 0 - code;    /* Convert to positive */
        }
    }

    /* Normalize the reason and len */
    if (reason) {
        if (0 == len) {
            reason = NULL;
        } else if (len == SIZE_MAX) {
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
    } else {
        len = 0;
    }

    /* Handle the special 0 code case */
    if (0 == code) {
        /* You can't have a code of 0 and expect to send reason text. */
        if ((NULL != reason) || (0 < len)) {
            return CWSE_INVALID_CLOSE_REASON_CODE;
        }
    } else {
        if (false == is_close_code_valid(code)) {
            return CWSE_INVALID_CLOSE_REASON_CODE;
        }
    }

    /* Transfer the results back only if successful */
    *_code = code;
    *_len = len;
    *_opts = opts;
    *_reason = reason;

    return CWSE_OK;
}


static int _check_curl_version(void)
{
    const curl_version_info_data *curl_ver = NULL;
    curl_ver = curl_version_info(CURLVERSION_NOW);

    if (curl_ver->version_num < CURLWS_MIN_VERSION) {
        fprintf(stderr, "ERROR: CURL version '%s'. At least '%s' is required for WebSocket to work reliably",
                curl_ver->version, CURLWS_MIN_VERSION_STRING);
        return -1;
    }
    return 0;
}


/*----------------------------------------------------------------------------*/
/*                      Internal Configuration Functions                      */
/*----------------------------------------------------------------------------*/
static int _config_url(CWS *priv, const struct cws_config *config)
{
    if (config->url) {
        priv->cfg.url = cws_rewrite_url(config->url);
        if (priv->cfg.url) {
            if (CURLE_OK == curl_easy_setopt(priv->easy, CURLOPT_URL, priv->cfg.url)) {
                return 0;
            }
        }
    }

    return -1;
}


static int _config_redirects(CWS *priv, const struct cws_config *config)
{
    CURLcode rv = CURLE_OK;

    /* Invalid, too low */
    if (config->max_redirects < -1) {
        return -1;
    }

    /* Setup redirect limits. */
    if (0 != config->max_redirects) {
        rv |= curl_easy_setopt(priv->easy, CURLOPT_FOLLOWLOCATION, 1L);
        rv |= curl_easy_setopt(priv->easy, CURLOPT_MAXREDIRS, config->max_redirects);
    }

    return (CURLE_OK == rv) ? 0 : -1;
}


static int _config_ip_version(CWS *priv, const struct cws_config *config)
{
    CURLcode rv = CURLE_OK;

    if (0 != config->ip_version) {
        long resolve;

        if (4 == config->ip_version) {
            resolve = CURL_IPRESOLVE_V4;
        } else if (6 == config->ip_version) {
            resolve = CURL_IPRESOLVE_V6;
        } else {
            return -1;
        }

        rv |= curl_easy_setopt(priv->easy, CURLOPT_IPRESOLVE, resolve);
    }

    return (CURLE_OK == rv) ? 0 : -1;
}


static int _config_interface(CWS *priv, const struct cws_config *config)
{
    CURLcode rv = CURLE_OK;

    if (NULL != config->interface) {
        rv |= curl_easy_setopt(priv->easy, CURLOPT_INTERFACE, config->interface);
    }

    return (CURLE_OK == rv) ? 0 : -1;
}


static int _config_security(CWS *priv, const struct cws_config *config)
{
    /* Force a reasonably modern version of TLS */
    if (CURLE_OK != curl_easy_setopt(priv->easy, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2)) {
        return -1;
    }

    if (0 == config->insecure_ok) {
        return 0;
    }

    /* If you **really** must run in insecure mode, ok... but seriously this
     * is dangerous. */

    if (0x7269736b == config->insecure_ok) {
        if (CURLE_OK != curl_easy_setopt(priv->easy, CURLOPT_SSL_VERIFYPEER, 0L)) {
            return -1;
        }
        if (CURLE_OK != curl_easy_setopt(priv->easy, CURLOPT_SSL_VERIFYHOST, 0L)) {
            return -1;
        }

        return 0;
    }

    return -1;
}



static int _config_verbosity(CWS *priv, const struct cws_config *config)
{
    CURLcode rv = CURLE_OK;

    if ((config->verbose < 0) || (3 < config->verbose)) {
        return -1;
    }

    priv->cfg.verbose = config->verbose;
    if (1 < config->verbose) {
        rv |= curl_easy_setopt(priv->easy, CURLOPT_VERBOSE, 1L);
    }

    return (CURLE_OK == rv) ? 0 : -1;
}


static int _config_ws_workarounds(CWS *priv, const struct cws_config *config)
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
    if (1 == config->explicit_expect) {
        p = curl_slist_append(priv->headers, "Expect: 101");
        if (!p) {
            return -1;
        }
        priv->headers = p;
    } else if (0 != config->explicit_expect) {
        return -1;
    }

    /*
     * CURLOPT_UPLOAD=1 without a size implies in:
     *     Transfer-Encoding: chunked
     * but we don't want that, rather unmodified (raw) bites as we're
     * doing the websockets framing ourselves. Force nothing.
     */
    p = curl_slist_append(priv->headers, "Transfer-Encoding:");
    if (!p) {
        return -1;
    }
    priv->headers = p;

    /* END: work around CURL to get WebSocket. */

    return (CURLE_OK == rv) ? 0 : -1;
}


static int _config_memorypool(CWS *priv, const struct cws_config *config)
{
    size_t max_payload_size;

    priv->cfg.max_payload_size = 1024;
    if (config->max_payload_size) {
        priv->cfg.max_payload_size = config->max_payload_size;
    }

    /* The largest frame size, so allocate this amount. */
    max_payload_size = priv->cfg.max_payload_size + WS_FRAME_HEADER_MAX;

    priv->mem_cfg.data_block_size = send_get_memory_needed(max_payload_size);
    priv->mem_cfg.control_block_size = send_get_memory_needed(WS_CTL_FRAME_MAX);

    priv->mem = mem_init_pool(&priv->mem_cfg);

    return 0;
}


static int _config_ws_key(CWS *priv)
{
    const char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const char header[] = "Sec-WebSocket-Key: ";

    struct curl_slist *tmp = NULL;
    uint8_t sha1_value[20];
    uint8_t random_value[16];
    char b64_key[25];           /* b64(16 bytes) = 24 bytes + 1 for '\0' */
 
    /* For the next two values: -1 because only 1 trailing '\0' is needed */
    char combined[sizeof(b64_key) + sizeof(guid) - 1];
    char send[sizeof(header) + sizeof(b64_key) - 1];

    cws_random(priv, random_value, sizeof(random_value));
    cws_encode_base64(random_value, sizeof(random_value), b64_key);

    snprintf(send, sizeof(send), "%s%s", header, b64_key);

    tmp = curl_slist_append(priv->headers, send);
    if (!tmp) {
        return -1;
    }

    priv->headers = tmp;
    snprintf(combined, sizeof(combined), "%s%s", b64_key, guid);

    cws_sha1(combined, strlen(combined), sha1_value);
    cws_encode_base64(sha1_value, sizeof(sha1_value), priv->expected_key_header);

    return 0;
}

static int _config_ws_protocols(CWS *priv, const struct cws_config *config)
{
    struct curl_slist *tmp;
    char *buf;

    if (!config->websocket_protocols) {
        return 0;
    }

    buf = cws_strmerge("Sec-WebSocket-Protocol: ", config->websocket_protocols);
    if (!buf) {
        return -1;
    }

    tmp = curl_slist_append(priv->headers, buf);
    free(buf);

    if (!tmp) {
        return -1;
    }

    priv->headers = tmp;

    priv->cfg.ws_protocols_requested = cws_strdup(config->websocket_protocols);
    if (!priv->cfg.ws_protocols_requested) {
        return -1;
    }

    return 0;
}


static int _config_http_headers(CWS *priv, const struct cws_config *config)
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
    CURLcode rv = CURLE_OK;
    struct curl_slist *p = NULL;
    struct curl_slist *extra = NULL;

    /* Add the required headers */
    for (size_t i = 0; i < sizeof(ws_headers) / sizeof(ws_headers[0]); i++) {
        p = curl_slist_append(priv->headers, ws_headers[i]);
        if (!p) {
            return -1;
        }
        priv->headers = p;
    }


    /* Add the extra headers */
    extra = config->extra_headers;
    while (extra) {
        /* Validate the extra header is ok */
        for (size_t i = 0; i < sizeof(disallowed) / sizeof(disallowed[0]); i++) {
            if (cws_has_prefix(extra->data, strlen(extra->data), disallowed[i])) {
                return -1;
            }
        }

        p = curl_slist_append(priv->headers, extra->data);
        if (!p) {
            return -1;
        }
        priv->headers = p;
        extra = extra->next;
    }

    rv |= curl_easy_setopt(priv->easy, CURLOPT_HTTPHEADER, priv->headers);

    return (CURLE_OK == rv) ? 0 : -1;
}
