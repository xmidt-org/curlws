/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
const char *cws_trim(const char *s, size_t *len)
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


bool cws_has_prefix(const char *s, size_t len, const char *prefix)
{
    size_t prefixlen = strlen(prefix);

    s = cws_trim(s, &len);

    if (len < prefixlen) {
        return false;
    }

    return cws_strncasecmp(s, prefix, prefixlen) == 0;
}


char *cws_rewrite_url(const char *url)
{
    size_t ws_len  = 5;
    size_t wss_len = 6;
    size_t url_len = strlen(url);
    char *rv;

    /* ws:// -> http:// is +2, wss:// -> https:// is +2, +1 for '\0'.
     * This is simpler & only 'wastes' 2 bytes. */
    rv = (char *) malloc(url_len + 3);
    if (rv) {
        /* with the starting '\0' strcat can do it's work. */
        rv[0] = '\0';

        if (strncmp(url, "ws://", ws_len) == 0) {
            snprintf(rv, (url_len + 3), "http://%s", &url[ws_len]);
        } else if (strncmp(url, "wss://", wss_len) == 0) {
            snprintf(rv, (url_len + 3), "https://%s", &url[wss_len]);
        } else {
            memcpy(rv, url, url_len + 1);
        }
    }

    return rv;
}


size_t cws_strnlen(const char *s, size_t maxlen)
{
    for (size_t i = 0; i < maxlen; i++) {
        if ('\0' == s[i]) {
            return i;
        }
    }

    return maxlen;
}


char *cws_strdup(const char *s)
{
    return cws_strndup(s, SIZE_MAX);
}


char *cws_strndup(const char *s, size_t maxlen)
{
    char *rv = NULL;

    if (s && (0 < maxlen)) {
        size_t len = cws_strnlen(s, maxlen);

        rv = (char *) malloc(len + 1); /* +1 for trailing '\0' */
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


char *cws_strmerge(const char *s1, const char *s2)
{
    size_t l1, l2;
    char *p;

    l1 = strlen(s1);
    l2 = strlen(s2);

    p = (char *) malloc(l1 + l2 + 1);
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
