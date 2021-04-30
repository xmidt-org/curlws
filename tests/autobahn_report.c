/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include "../src/curlws.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
static void main_loop(CURLM *multi);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
    char url[4096];
    const char *base_url;
    int opt = 1;

    struct cws_config cfg;
    CWS *ws;
    CURLM *multi;

    memset(&cfg, 0, sizeof(cfg));

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            fprintf(stderr,
                    "Usage:\n"
                    "\t%s <base_url>\n"
                    "\n"
                    "Example:\n"
                    "\t%s ws://localhost:9001\n"
                    "\t%s wss://localhost:9001\n"
                    "\n",
                    argv[0], argv[0], argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-vv") == 0) {
            cfg.verbose = 3;
            opt = i + 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            cfg.verbose = 1;
            opt = i + 1;
        }
    }

    base_url = argv[opt];

    curl_global_init(CURL_GLOBAL_DEFAULT);

    snprintf(url, sizeof(url), "%s/updateReports?agent=curlws", base_url);
    cfg.url = url;
    cfg.expect = 1;

    ws = cws_create(&cfg);
    if (ws) {
        multi = curl_multi_init();
        if (multi) {
            if (CURLM_OK == cws_multi_add_handle(ws, multi)) {
                main_loop(multi);
                cws_multi_remove_handle(ws, multi);
            }
            curl_multi_cleanup(multi);
        }
        cws_destroy(ws);
    }
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void main_loop(CURLM *multi)
{
    int still_running = 0;

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

        /* See how the transfers went */
        while ((msg = curl_multi_info_read(multi, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                fprintf(stderr, "HTTP completed with status %d '%s'",
                    msg->data.result, curl_easy_strerror(msg->data.result));
            }
        }
    } while (still_running);
}
