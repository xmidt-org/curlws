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
static CWScode _normalize_close_inputs(int*, int*, const char**, size_t*);

static bool _validate_config(const struct cws_config*);
static struct curl_slist* _cws_calculate_websocket_key(CWS*, struct curl_slist*);
static struct curl_slist* _cws_add_websocket_protocols(CWS*, struct curl_slist*, const char*);
static struct curl_slist* _cws_add_headers(struct curl_slist*,
                                           const struct cws_config*,
                                           const char*[], size_t);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

CWS *cws_create(const struct cws_config *config)
{
    const char *ws_headers[] = {
         "Connection: Upgrade",
         "Upgrade: websocket",
         "Sec-WebSocket-Version: 13",
    };

    CWS *priv = NULL;
    const curl_version_info_data *curl_ver = NULL;
    struct curl_slist *headers = NULL;
    struct curl_slist *tmp = NULL;
    size_t max_payload_size;

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

    priv->cfg.max_payload_size = 1024;
    if (config->max_payload_size) {
        priv->cfg.max_payload_size = config->max_payload_size;
    }

    /* The largest frame size, so allocate this amount. */
    max_payload_size = priv->cfg.max_payload_size + WS_FRAME_HEADER_MAX;

    priv->mem_cfg.data_block_size = send_get_memory_needed(max_payload_size);
    priv->mem_cfg.control_block_size = send_get_memory_needed(WS_CTL_FRAME_MAX);

    priv->mem = mem_init_pool(&priv->mem_cfg);
    if (!priv) { goto error; }

    populate_callbacks(&priv->cb, config);

    priv->easy = curl_easy_init();
    if (!priv->easy) { goto error; }

    priv->cfg.user = config->user;

    /* Place things here that are "ok" for a user to overwrite. */

    /* Force a reasonably modern version of TLS */
    curl_easy_setopt(priv->easy, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    /* Setup redirect limits. */
    if (0 != config->max_redirects) {
        curl_easy_setopt(priv->easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(priv->easy, CURLOPT_MAXREDIRS, config->max_redirects);
    }

    if (NULL != config->interface) {
        curl_easy_setopt(priv->easy, CURLOPT_INTERFACE, config->interface);
    }

    if (priv->cb.configure_fn) {
        (*priv->cb.configure_fn)(priv->cfg.user, priv, priv->easy);
    }

    curl_easy_setopt(priv->easy, CURLOPT_PRIVATE, priv);

    header_init(priv);
    receive_init(priv);
    send_init(priv);

    priv->cfg.url = cws_rewrite_url(config->url);
    if (!priv->cfg.url) { goto error; }
    curl_easy_setopt(priv->easy, CURLOPT_URL, priv->cfg.url);

    priv->cfg.verbose = config->verbose;
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

    /* We need to close connections and not allow reuse. */
    curl_easy_setopt(priv->easy, CURLOPT_FORBID_REUSE, 1L);

    /* We need to freshly connect each time. */
    curl_easy_setopt(priv->easy, CURLOPT_FRESH_CONNECT, 1L);

    /*
     * CURLOPT_UPLOAD=1 with HTTP/1.1 implies:
     *     Expect: 100-continue
     * but we don't want that, rather 101. Then force: 101.
     */
    if (1 == config->explicit_expect) {
        tmp = curl_slist_append(headers, "Expect: 101");
        if (!tmp) { goto error; }
        headers = tmp;
    }

    /*
     * CURLOPT_UPLOAD=1 without a size implies in:
     *     Transfer-Encoding: chunked
     * but we don't want that, rather unmodified (raw) bites as we're
     * doing the websockets framing ourselves. Force nothing.
     */
    tmp = curl_slist_append(headers, "Transfer-Encoding:");
    if (!tmp) { goto error; }
    headers = tmp;

    /* END: work around CURL to get WebSocket. */

    /* Calculate the unique key pair. */
    tmp = _cws_calculate_websocket_key(priv, headers);
    if (!tmp) { goto error; }

    tmp = _cws_add_websocket_protocols(priv, headers, config->websocket_protocols);
    if (!tmp) { goto error; }

    /* regular mandatory WebSockets headers (ws_headers) and any custom ones
     * specified by the config */

    tmp = _cws_add_headers(headers, config, ws_headers, sizeof(ws_headers)/sizeof(char*));
    if (!tmp) { goto error; }
    headers = tmp;

    curl_easy_setopt(priv->easy, CURLOPT_HTTPHEADER, headers);
    priv->headers = headers;

    return priv;

error:
    if (headers) {
        curl_slist_free_all(headers);
    }

    cws_destroy(priv);
    return NULL;
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
    return frame_sender_control(priv, CWS_PING, data, len);
}


CWScode cws_pong(CWS *priv, const void *data, size_t len)
{
    return frame_sender_control(priv, CWS_PONG, data, len);
}


CWScode cws_close(CWS *priv, int code, const char *reason, size_t len)
{
    int options;
    CWScode rv;
    uint8_t buf[WS_CTL_PAYLOAD_MAX];    /* Limited by RFC6455 Section 5.5 */
    uint8_t *p = NULL;

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
    return data_block_sender(priv, CWS_BINARY, data, len);
}


CWScode cws_send_blk_text(CWS *priv, const char *s, size_t len)
{
    if (s) {
        if (len == SIZE_MAX) {
            len = strlen(s);
        }
    } else {
        len = 0;
    }

    if (((ssize_t) len) != utf8_validate(s, len)) {
        return CWSE_INVALID_UTF8;
    }

    return data_block_sender(priv, CWS_TEXT, s, len);
}


CWScode cws_send_strm_binary(CWS *priv, int info, const void *data, size_t len)
{
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(info);
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(len);
    return CWSE_OK;
}


CWScode cws_send_strm_text(CWS *priv, int info, const char *s, size_t len)
{
    IGNORE_UNUSED(priv);
    IGNORE_UNUSED(info);
    IGNORE_UNUSED(s);
    IGNORE_UNUSED(len);

    if (((ssize_t) len) != utf8_validate(s, len)) {
        return CWSE_INVALID_UTF8;
    }

    return CWSE_OK;
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
            if (((ssize_t) len) != utf8_validate(reason, len)) {
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
        for (size_t i = 0; i < sizeof(disallowed) / sizeof(disallowed[0]); i++) {
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

static struct curl_slist* _cws_calculate_websocket_key(CWS *priv, struct curl_slist *headers)
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

    tmp = curl_slist_append(headers, send);
    if (tmp) {
        snprintf(combined, sizeof(combined), "%s%s", b64_key, guid);

        cws_sha1(combined, strlen(combined), sha1_value);
        cws_encode_base64(sha1_value, sizeof(sha1_value), priv->expected_key_header);
    }

    return tmp;
}

static struct curl_slist* _cws_add_websocket_protocols(CWS *priv, struct curl_slist *headers,
                                                       const char *websocket_protocols)
{
    char *buf;

    if (!websocket_protocols) {
        return headers;
    }

    buf = cws_strmerge("Sec-WebSocket-Protocol: ", websocket_protocols);
    if (!buf) {
        return NULL;
    }

    headers = curl_slist_append(headers, buf);
    free(buf);

    if (headers) {
        priv->cfg.ws_protocols_requested = cws_strdup(websocket_protocols);
    }

    return headers;
}

static struct curl_slist* _cws_add_headers(struct curl_slist *headers,
                                           const struct cws_config *config,
                                           const char *list[], size_t len)
{
    struct curl_slist *p, *tmp;

    tmp = NULL;

    for (size_t i = 0; i < len; i++) {
        tmp = curl_slist_append(headers, list[i]);
        if (!tmp) {
            return NULL;
        }
        headers = tmp;
    }

    p = config->extra_headers;
    while (p) {
        tmp = curl_slist_append(headers, p->data);
        if (!tmp) {
            return NULL;
        }

        headers = tmp;
        p = p->next;
    }

    return headers;
}
