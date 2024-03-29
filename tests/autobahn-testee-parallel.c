/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
/* c-mode: linux-4 */
#include <ctype.h>
#include <curlws/curlws.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ws_list {
    CWS *ws;
    int exitval;
    time_t exit_started;
    struct ws_list *next;
    int test_number;
};

struct myapp_ctx {
    struct ws_list *head;
    CURLM *multi;
    // CWS *ws;
    // int exitval;
    // time_t exit_started;
};

static bool verbose = false;

static void INF(const char *fmt, ...)
{
    if (verbose) {
        va_list args;

        va_start(args, fmt);
        fprintf(stderr, "INFO: ");
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
    }
}

static void ERR(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/*
 * This is a traditional curl_multi app, see:
 *
 * https://curl.haxx.se/libcurl/c/multi-app.html
 *
 * replace this with your own main loop integration
 */
static void a_main_loop(struct myapp_ctx *ctx)
{
    struct ws_list *p = ctx->head;
    CURLM *multi      = ctx->multi;
    int still_running = 0;

    curl_multi_perform(multi, &still_running);

    do {
        CURLMsg *msg;
        struct timeval timeout;
        fd_set fdread, fdwrite, fdexcep;
        CURLMcode mc;
        int msgs_left, rc;
        int maxfd       = -1;
        long curl_timeo = -1;
        bool exit;
        time_t now;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);

        timeout.tv_sec  = 0;
        timeout.tv_usec = 200000;

        curl_multi_timeout(multi, &curl_timeo);

        if (curl_timeo >= 0) {
            timeout.tv_sec  = curl_timeo / 1000;
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }

        if (timeout.tv_sec > 1)
            timeout.tv_sec = 1;

        mc = curl_multi_fdset(multi, &fdread, &fdwrite, &fdexcep, &maxfd);
        if (mc != CURLM_OK) {
            ERR("curl_multi_fdset() failed, code %d '%s'.",
                mc, curl_multi_strerror(mc));
            break;
        }

        rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

        switch (rc) {
            case -1:
                /* select error */
                break;
            case 0:  /* timeout */
            default: /* action */
                curl_multi_perform(multi, &still_running);
                break;
        }

        // printf( "rc: %d, still_running: %d, select timeout: %ld, %ld\n", rc, still_running, timeout.tv_sec, timeout.tv_usec);

        /* See how the transfers went */
        while ((msg = curl_multi_info_read(multi, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                curl_multi_remove_handle(multi, msg->easy_handle);
                INF("HTTP completed with status %d '%s'",
                    msg->data.result, curl_easy_strerror(msg->data.result));
            }
        }

        exit = true;
        now  = time(NULL);
        p    = ctx->head;
        while (p) {
            // printf("exit_started %ld, %ld\n", p->exit_started, (now - p->exit_started));
            if (!p->exit_started || (now - p->exit_started < 2)) {
                exit = false;
                p    = NULL;
            } else {
                p = p->next;
            }
        }
        // printf("exit: %d\n", exit);

        if (exit) {
            break;
        }
    } while (still_running);
    /*
    INF("quit main loop still_running=%d, ctx->p->exit_started=%ld (%ld seconds)",
        still_running,
        ctx->exit_started,
        (ctx->exit_started > 0) ? time(NULL) - ctx->exit_started : 0);
    */
}

/* https://www.w3.org/International/questions/qa-forms-utf-8 */
static bool check_utf8(const char *text)
{
    const uint8_t *bytes = (const uint8_t *) text;
    while (*bytes) {
        const uint8_t c = bytes[0];

        /* ascii: [\x09\x0A\x0D\x20-\x7E] */
        if ((c >= 0x20 && c <= 0x7e) || c == 0x09 || c == 0x0a || c == 0x0d) {
            bytes += 1;
            continue;
        }

        /* autobahnsuite says 0x7f is valid */
        if (c == 0x7f) {
            bytes += 1;
            continue;
        }

#define VALUE_BYTE_CHECK(x) ((x >= 0x80) && (x <= 0xbf))

        /* non-overlong 2-byte: [\xC2-\xDF][\x80-\xBF] */
        if (c >= 0xc2 && c <= 0xdf) {
            if (VALUE_BYTE_CHECK(bytes[1])) {
                bytes += 2;
                continue;
            }
        }

        /* excluding overlongs: \xE0[\xA0-\xBF][\x80-\xBF] */
        if (c == 0xe0) {
            const uint8_t d = bytes[1];
            if ((d >= 0xa0) && (d <= 0xbf)) {
                if (VALUE_BYTE_CHECK(bytes[2])) {
                    bytes += 3;
                    continue;
                }
            }
        }

        /* straight 3-byte: [\xE1-\xEC\xEE\xEF][\x80-\xBF]{2} */
        if ((c >= 0xe1 && c <= 0xec) || c == 0xee || c == 0xef) {
            if (VALUE_BYTE_CHECK(bytes[1]) && VALUE_BYTE_CHECK(bytes[2])) {
                bytes += 3;
                continue;
            }
        }

        /* excluding surrogates: \xED[\x80-\x9F][\x80-\xBF] */
        if (c == 0xed) {
            const uint8_t d = bytes[1];
            if (d >= 0x80 && d <= 0x9f) {
                if (VALUE_BYTE_CHECK(bytes[2])) {
                    bytes += 3;
                    continue;
                }
            }
        }

        /* planes 1-3: \xF0[\x90-\xBF][\x80-\xBF]{2} */
        if (c == 0xf0) {
            const uint8_t d = bytes[1];
            if (d >= 0x90 && d <= 0xbf) {
                if (VALUE_BYTE_CHECK(bytes[2]) && VALUE_BYTE_CHECK(bytes[3])) {
                    bytes += 4;
                    continue;
                }
            }
        }
        /* planes 4-15: [\xF1-\xF3][\x80-\xBF]{3} */
        if (c >= 0xf1 && c <= 0xf3) {
            if (VALUE_BYTE_CHECK(bytes[1]) && VALUE_BYTE_CHECK(bytes[2]) && VALUE_BYTE_CHECK(bytes[3])) {
                bytes += 4;
                continue;
            }
        }

        /* plane 16: \xF4[\x80-\x8F][\x80-\xBF]{2} */
        if (c == 0xf4) {
            const uint8_t d = bytes[1];
            if (d >= 0x80 && d <= 0x8f) {
                if (VALUE_BYTE_CHECK(bytes[2]) && VALUE_BYTE_CHECK(bytes[3])) {
                    bytes += 4;
                    continue;
                }
            }
        }

        INF("failed unicode byte #%zd '%s'", (const char *) bytes - text, text);
        return false;
    }

    return true;
}

static int on_connect(void *data, CWS *ws, const char *websocket_protocols)
{
    INF("connected, websocket_protocols='%s'", websocket_protocols);
    (void) data;
    (void) ws;

    return 0;
}

static int on_text(void *data, CWS *ws, const char *text, size_t len)
{
    if (0 < len) {
        char *tmp = (char *) malloc(len + 1);
        memcpy(tmp, text, len);
        tmp[len] = '\0';
        if (!check_utf8(tmp)) {
            cws_close(ws, 1007, "invalid UTF-8", SIZE_MAX);
            return 0;
        }
        free(tmp);
    }

    INF("TEXT %zd bytes={ '%s' }", len, text);
    cws_send_blk_text(ws, text, len);
    (void) data;

    return 0;
}

static int on_binary(void *data, CWS *ws, const void *mem, size_t len)
{
    const uint8_t *bytes = mem;
    size_t i;

    if (verbose) {
        INF("BINARY=%zd bytes {", len);
        for (i = 0; i < len; i++) {
            uint8_t b = bytes[i];
            if (isprint(b))
                fprintf(stderr, " %#04x(%c)", b, b);
            else
                fprintf(stderr, " %#04x", b);
        }
        fprintf(stderr, "\n}\n");
    }

    cws_send_blk_binary(ws, mem, len);
    (void) data;

    return 0;
}

static int on_ping(void *data, CWS *ws, const void *reason, size_t len)
{
    INF("PING %zd bytes='%.*s'", len, (int) len, (const char *) reason);
    cws_pong(ws, reason, len);
    (void) data;

    return 0;
}

static int on_pong(void *data, CWS *ws, const void *reason, size_t len)
{
    INF("PONG %zd bytes='%.*s'", len, (int) len, (const char *) reason);
    (void) data;
    (void) ws;

    return 0;
}

static int on_close(void *data, CWS *ws, int reason, const char *reason_text, size_t len)
{
    struct ws_list *ctx = data;

    INF("CLOSE=%4d %zd bytes '%s'", reason, len, reason_text);
    ctx->exit_started = time(NULL);

    if (0 < len) {
        char *tmp = (char *) malloc(len + 1);
        memcpy(tmp, reason_text, len);
        tmp[len] = '\0';
        if (!check_utf8(reason_text)) {
            cws_close(ws, 1007, "invalid UTF-8", SIZE_MAX);
            return 0;
        }
        free(tmp);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    const char *base_url;
    unsigned start_test, end_test, current_test;
    int i;
    int exitval = 0;
    int opt     = 1;
    struct myapp_ctx myapp_ctx;
    struct ws_list *p = NULL;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fprintf(stderr,
                    "Usage:\n"
                    "\t%s <base_url> <start_test> <end_test>\n"
                    "\n"
                    "First install autobahn test suite:\n"
                    "\tpip2 install --user autobahntestsuite\n"
                    "Then start autobahn:\n"
                    "\t~/.local/bin/wstest -m fuzzingserver\n"
                    "\n"
                    "Example:\n"
                    "\t%s ws://localhost:9001 1 260\n"
                    "\t%s wss://localhost:9001 1 10\n"
                    "\n",
                    argv[0], argv[0], argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            opt     = i + 1;
        }
    }

    if (opt + 2 > argc) {
        fprintf(stderr, "ERROR: missing url start and end test number. See -h.\n");
        return EXIT_FAILURE;
    }

    base_url   = argv[opt];
    start_test = atoi(argv[opt + 1]);
    end_test   = start_test;
    if (opt + 3 <= argc) {
        end_test = atoi(argv[opt + 2]);
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    memset(&myapp_ctx, 0, sizeof(myapp_ctx));

    /*
     * This is a traditional curl_multi app, see:
     *
     * https://curl.haxx.se/libcurl/c/multi-app.html
     */
    myapp_ctx.multi = curl_multi_init();
    if (!myapp_ctx.multi) {
        printf("invalid multi handle\n");
        goto error_multi;
    }

    for (current_test = start_test; current_test <= end_test; current_test++) {
        char test_url[4096];
        struct cws_config cfg;
        CURLMcode rv;

        if (!p) {
            p              = (struct ws_list *) calloc(1, sizeof(struct ws_list));
            myapp_ctx.head = p;
        } else {
            p->next = (struct ws_list *) calloc(1, sizeof(struct ws_list));
            p       = p->next;
        }
        p->test_number = current_test;

        memset(&cfg, 0, sizeof(cfg));
        cfg.url              = test_url;
        cfg.verbose          = (true == verbose) ? 1 : 0;
        cfg.on_connect       = on_connect;
        cfg.on_text          = on_text;
        cfg.on_binary        = on_binary;
        cfg.on_ping          = on_ping;
        cfg.on_pong          = on_pong;
        cfg.on_close         = on_close;
        cfg.user             = p;
        cfg.expect           = 1;
        cfg.max_payload_size = 131070;

        // fprintf(stderr, "TEST: %u\n", current_test);

        snprintf(test_url, sizeof(test_url),
                 "%s/runCase?case=%u&agent=curlws",
                 base_url, current_test);

        p->ws = cws_create(&cfg);
        INF("ws: %p, url: %s", (void *) p->ws, test_url);
        if (!p->ws)
            goto error_ws;

        rv = cws_multi_add_handle(p->ws, myapp_ctx.multi);
        if (CURLM_OK != rv) {
            printf("cws_multi_add_handle(): %d\n", rv);
        }
    }

    a_main_loop(&myapp_ctx);

error_ws:
    exitval = 0;
    p       = myapp_ctx.head;
    while (p) {
        struct ws_list *next;
        cws_multi_remove_handle(p->ws, myapp_ctx.multi);
        exitval |= p->exitval;
        next = p->next;
        cws_destroy(p->ws);
        printf("TEST %d - %d\n", p->test_number, p->exitval);
        free(p);
        p = next;
    }

    curl_multi_cleanup(myapp_ctx.multi);

error_multi:

    curl_global_cleanup();

    return exitval;
}
