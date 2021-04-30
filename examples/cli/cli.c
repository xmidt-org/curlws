/*
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 * SPDX-License-Identifier: MIT
 */
#include "../../src/curlws.h"

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */
struct my_cfg {
    long ip_resolve;
    long tls_version;
    bool insecure;
    const char *interface;
};

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
static int shutdown_ws = 0;

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void handle_sigint(int);
static void main_loop(CWS*, CURLM*);
static bool is_opt(const char*, const char*, const char*);

static CURLcode configure(void*, CWS*, CURL*);
static int on_connect(void*, CWS*, const char*);
static int on_text(void*, CWS*, const char*, size_t);
static int on_binary(void*, CWS*, const void*, size_t);
static int on_close(void*, CWS*, int, const char*, size_t);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
    struct cws_config cfg;
    CWS *ws;
    CURLM *multi;
    long max_redirs = -1;
    struct my_cfg custom;

    memset(&custom, 0, sizeof(custom));
    custom.ip_resolve = CURL_IPRESOLVE_WHATEVER;
    custom.tls_version = CURL_SSLVERSION_MAX_DEFAULT;

    /* Select the defaults for curlws & overwrite them as needed. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.user = &custom;
    cfg.configure = configure;

    /* Provide the four most useful callbacks for reference. */
    cfg.on_connect = on_connect;
    cfg.on_text    = on_text;
    cfg.on_binary  = on_binary;
    cfg.on_close   = on_close;

    /* Very simple args parser. */
    for (int i = 1; i < argc; i++) {
        if (is_opt(argv[i], "-4", NULL)) {
            custom.ip_resolve = CURL_IPRESOLVE_V4;
        } else if (is_opt(argv[i], "-6", NULL)) {
            custom.ip_resolve = CURL_IPRESOLVE_V6;
        } else if (is_opt(argv[i], NULL, "--expect-101")) {
            cfg.expect = 1;
        } else if (is_opt(argv[i], "-h", "--help")) {
            printf("Usage: %s [options...] <url>\n"
                   " -4                       Resolve names to IPv4 addresses\n"
                   " -6                       Resolve names to IPv6 addresses\n"
                   "     --expect-101         Set the Expect: 101 (some servers need this, others do not)\n"
                   " -h, --help               This help text\n"
                   " -H, --header    <header> Pass custom header to server\n"
                   "     --interface <name>   Use network INTERFACE (or address)\n"
                   " -k, --insecure           Allow insecure server connections when using SSL\n"
                   " -L, --location           Follow redirects\n"
                   "     --max-payload <num>  Maximum payload size to send\n"
                   "     --max-redirs <num>   Maximum number of redirects allowed\n"
                   "     --tlsv1.2            Set the maximum TLS version (useful since Wireshare can only decode tls1.2\n"
                   " -v  --verbose            Verbose debugging in curlws is enabled, repeat for more\n"
                   "     --ws-protos <name>   List of websocket protocols to negotiate\n",
                   argv[0]);
            return 0;
        } else if (is_opt(argv[i], "-H", "--header")) {
            i++;
            cfg.extra_headers = curl_slist_append(cfg.extra_headers, argv[i]);
        } else if (is_opt(argv[i], NULL, "--interface")) {
            i++;
            custom.interface = argv[i];
        } else if (is_opt(argv[i], "-k", "--insecure")) {
            custom.insecure = true;
        } else if (is_opt(argv[i], "-L", "--location")) {
            cfg.max_redirects = -1;
        } else if (is_opt(argv[i], NULL, "--max-payload")) {
            i++;
            cfg.max_payload_size = atol(argv[i]);
        } else if (is_opt(argv[i], NULL, "--max-redirs")) {
            i++;
            max_redirs = atol(argv[i]);
        } else if (is_opt(argv[i], NULL, "--tlsv1.2")) {
            custom.tls_version = CURL_SSLVERSION_MAX_TLSv1_2;
        } else if (is_opt(argv[i], "-v", "--verbose")) {
            cfg.verbose++;
        } else if (is_opt(argv[i], NULL, "--ws_protos")) {
            i++;
            cfg.websocket_protocols = argv[i];
        } else {
            cfg.url = argv[i];
            break;
        }
    }

    if (-1 == cfg.max_redirects) {
        if (0 != max_redirs) {
            cfg.max_redirects = max_redirs;
        }
    }

    signal(SIGINT, handle_sigint);

    /* Create the curl global context. */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Create the websocket configuration */
    ws = cws_create(&cfg);
    if (ws) {

        /* Create the curlm instance. */
        multi = curl_multi_init();
        if (multi) {

            /* Add the websocket to the multi handler. */
            if (CURLM_OK == cws_multi_add_handle(ws, multi)) {
                /* Run the main loop until we are done. */
                main_loop(ws, multi);
                cws_multi_remove_handle(ws, multi);
            }
            curl_multi_cleanup(multi);
        }
        cws_destroy(ws);
    }

    return 0;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void handle_sigint(int sig)
{
    shutdown_ws = 1;
}

static void main_loop(CWS *ws, CURLM *multi)
{
    int still_running = 0;

    /* This is the standard curl multi handling loop. */
    curl_multi_perform(multi, &still_running);

    do {
        CURLMsg *msg;
        struct timeval timeout;
        fd_set fdread, fdwrite, fdexcep;
        CURLMcode mc;
        int msgs_left, rc;
        int maxfd = -1;
        long curl_timeo = -1;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);

        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        curl_multi_timeout(multi, &curl_timeo);

        if (curl_timeo >= 0) {
            timeout.tv_sec = curl_timeo / 1000;
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }

        if (timeout.tv_sec > 1)
            timeout.tv_sec = 1;

        mc = curl_multi_fdset(multi, &fdread, &fdwrite, &fdexcep, &maxfd);
        if (mc != CURLM_OK) {
            fprintf(stderr, "curl_multi_fdset() failed, code %d '%s'.",
                mc, curl_multi_strerror(mc));
            break;
        }

        rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

        switch(rc) {
        case -1:
            /* select error */
            break;
        case 0: /* timeout */
        default: /* action */
            curl_multi_perform(multi, &still_running);
            break;
        }

        if (1 == shutdown_ws) {
            cws_close(ws, 1001, "Program stopping.", SIZE_MAX);
            shutdown_ws = 2;
        }

        /* See how the transfers went */
        while ((msg = curl_multi_info_read(multi, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                fprintf(stderr, "HTTP completed with status %d '%s'",
                    msg->data.result, curl_easy_strerror(msg->data.result));
            }
        }
    } while (still_running);
}


static CURLcode configure(void *user, CWS *ws, CURL *easy)
{
    struct my_cfg *cfg = (struct my_cfg*) user;
    CURLcode rv;

    rv  = curl_easy_setopt(easy, CURLOPT_IPRESOLVE, cfg->ip_resolve);
    rv |= curl_easy_setopt(easy, CURLOPT_SSLVERSION, cfg->tls_version);

    if (cfg->insecure) {
        rv |= curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
        rv |= curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
    }

    if (cfg->interface) {
        rv |= curl_easy_setopt(easy, CURLOPT_INTERFACE, cfg->interface);
    }

    return rv;
}


/* A simple options parser helper. */
static bool is_opt(const char *in, const char *s1, const char *s2)
{
    if (s1 && s2) {
        return ((0 == strcmp(in, s1)) || (0 == strcmp(in, s2))) ? true : false;
    } else if (s2) {
        return (0 == strcmp(in, s2)) ? true : false;
    } else if (s1) {
        return (0 == strcmp(in, s1)) ? true : false;
    }
    return false;
}


/* The custom on_connect handler for this instance of the websocket code. */
static int on_connect(void *user, CWS *ws, const char *protos)
{
    printf("on_connect(user, ws, '%s')\n", (NULL == protos) ? "" : protos );
    (void) user;
    (void) ws;

    /* TODO: ADD YOUR CODE HERE! */

    return 0;
}

/* The custom on_text handler for this instance of the websocket code. */
static int on_text(void *user, CWS *ws, const char *text, size_t len)
{
    printf("on_text(user, ws, '%.*s', %zd)\n", (int) len, text, len);
    (void) user;
    (void) ws;

    /* TODO: ADD YOUR CODE HERE! */

    return 0;
}


/* The custom on_binary handler for this instance of the websocket code. */
static int on_binary(void *user, CWS *ws, const void *buf, size_t len)
{
    printf("on_binary(user, ws, buf, %zd)\n", len);
    (void) user;
    (void) ws;
    (void) buf;

    /* TODO: ADD YOUR CODE HERE! */

    return 0;
}


/* The custom on_close handler for this instance of the websocket code. */
static int on_close(void *user, CWS *ws, int code, const char *reason, size_t len)
{
    printf("on_close(user, ws, %d, '%.*s', %zd)\n", code, (int) len, reason, len);
    (void) user;
    (void) ws;

    /* TODO: ADD YOUR CODE HERE! */

    return 0;
}


