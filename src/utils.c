/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

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
/* none */

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void cws_encode_base64(const void *in, const size_t len, char *out)
{
    static const uint8_t base64_map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    const uint8_t *input = (const uint8_t*) in;
    size_t i, o;
    uint8_t c;

    for (i = 0, o = 0; i + 3 <= len; i += 3) {
        /* 6 bits */
        c = (input[i] & 0xfc) >> 2;
        out[o++] = base64_map[c];

        /* 6 bits */
        c = (input[i] & 0x03) << 4;
        c |= (input[i + 1] & 0xf0) >> 4;
        out[o++] = base64_map[c];

        /* 6 bits */
        c = (input[i + 1] & 0x0f) << 2;
        c |= (input[i + 2] & 0xc0) >> 6;
        out[o++] = base64_map[c];

        /* 6 bits */
        c = input[i + 2] & 0x3f;
        out[o++] = base64_map[c];
    }

    if (i + 1 == len) {
        /* 6 bits */
        c = (input[i] & 0xfc) >> 2;
        out[o++] = base64_map[c];

        /* remaining 2 bits */
        c = (input[i] & 0x03) << 4;
        out[o++] = base64_map[c];

        /* filler '=' characters */
        out[o++] = base64_map[64];
        out[o++] = base64_map[64];
    } else if (i + 2 == len) {
        /* 6 bits */
        c = (input[i] & 0xfc) >> 2;
        out[o++] = base64_map[c];

        /* 6 bits */
        c = (input[i] & 0x03) << 4;
        c |= (input[i + 1] & 0xf0) >> 4;
        out[o++] = base64_map[c];

        /* remining 4 bits */
        c = (input[i + 1] & 0x0f) << 2;
        out[o++] = base64_map[c];

        /* filler '=' characters */
        out[o++] = base64_map[64];
    }
    out[o] = '\0';
}


const char* cws_trim(const char *s, size_t *len)
{
    size_t n = *len;

    while (n > 0 && isspace(s[0])) {
        s++;
        n--;
    }

    while (n > 0 && isspace(s[n - 1]))
        n--;

    *len = n;

    return s;
}


bool cws_has_prefix(const char *s, size_t len, const char *prefix) {
    size_t prefixlen = strlen(prefix);

    s = cws_trim(s, &len);

    if (len < prefixlen) {
        return false;
    }

    return cws_strncasecmp(s, prefix, prefixlen) == 0;
}


char* cws_rewrite_url(const char *url)
{
    size_t ws_len    = strlen("ws://");
    size_t wss_len   = strlen("wss://");
    size_t url_len   = strlen(url);
    char *rv;

    /* ws:// -> http:// is +2, wss:// -> https:// is +2, +1 for '\0'.
     * This is simpler & only 'wastes' 2 bytes. */
    rv = (char*) malloc( url_len + 3 );
    if (rv) {
        /* with the starting '\0' strcat can do it's work. */
        rv[0] = '\0';

        if (strncmp(url, "ws://", ws_len) == 0) {
            snprintf(rv, (url_len+3), "http://%s", &url[ws_len]);
        } else if (strncmp(url, "wss://", wss_len) == 0) {
            snprintf(rv, (url_len+3), "https://%s", &url[wss_len]);
        } else {
            memcpy(rv, url, url_len + 1);
        }
    }

    return rv;
}


char* cws_strdup(const char *s)
{
    char *rv = NULL;

    if (s) {
        size_t len;

        len = strlen(s) + 1;
        rv = (char*) malloc( len );
        if (rv) {
            memcpy(rv, s, len);
        }
    }

    return rv;
}


char* cws_strndup(const char *s, size_t n)
{
    char *rv = NULL;

    if ((NULL != s) && (0 < n)) {
        size_t len;

        len = strlen(s);
        if (n < len) {
            len = n;
        }

        rv = (char*) malloc(len + 1); /* +1 for trailing '\0' */
        if (rv) {
            memcpy(rv, s, len);
            rv[len] = '\0';
        }
    }

    return rv;
}


int cws_strncasecmp(const char *s1, const char *s2, size_t n)
{
    int c;

    if ((0 == n) || (s1 == s2)) {
        return 0;
    }

    do {
        c = tolower(*s1) - tolower(*s2);
        n--;
        s1++;
        s2++;
    } while (0 < n && 0 == c);

    return c;
}


char* cws_strmerge(const char *s1, const char *s2)
{
    size_t l1, l2;
    char *p;

    l1 = strlen(s1);
    l2 = strlen(s2);

    p = (char*) malloc( l1 + l2 + 1 );
    if (p) {
        memcpy(p, s1, l1);
        memcpy(&p[l1], s2, l2 + 1);
    }

    return p;
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */
