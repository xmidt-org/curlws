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
#include <ctype.h>
#include <string.h>

#include <openssl/evp.h>

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
void cws_sha1(const void *in, size_t len, void *out)
{
    static const EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;

    if (!md) {
        OpenSSL_add_all_digests();
        md = EVP_get_digestbyname("sha1");
    }

    ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, in, len);
    EVP_DigestFinal_ex(ctx, out, NULL);
    EVP_MD_CTX_destroy(ctx);
}


void cws_encode_base64(const void *in, const size_t len, char *out)
{
    static const char base64_map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    const uint8_t *input = (const uint8_t*) in;
    size_t i, o;
    uint8_t c;

    for (i = 0, o = 0; i + 3 <= len; i += 3) {
        c = (input[i] & (((1 << 6) - 1) << 2)) >> 2;
        out[o++] = base64_map[c];

        c = (input[i] & ((1 << 2) - 1)) << 4;
        c |= (input[i + 1] & (((1 << 4) - 1) << 4)) >> 4;
        out[o++] = base64_map[c];

        c = (input[i + 1] & ((1 << 4) - 1)) << 2;
        c |= (input[i + 2] & (((1 << 2) - 1) << 6)) >> 6;
        out[o++] = base64_map[c];

        c = input[i + 2] & ((1 << 6) - 1);
        out[o++] = base64_map[c];
    }

    if (i + 1 == len) {
        c = (input[i] & (((1 << 6) - 1) << 2)) >> 2;
        out[o++] = base64_map[c];

        c = (input[i] & ((1 << 2) - 1)) << 4;
        out[o++] = base64_map[c];

        out[o++] = base64_map[64];
        out[o++] = base64_map[64];
    } else if (i + 2 == len) {
        c = (input[i] & (((1 << 6) - 1) << 2)) >> 2;
        out[o++] = base64_map[c];

        c = (input[i] & ((1 << 2) - 1)) << 4;
        c |= (input[i + 1] & (((1 << 4) - 1) << 4)) >> 4;
        out[o++] = base64_map[c];

        c = (input[i + 1] & ((1 << 4) - 1)) << 2;
        out[o++] = base64_map[c];

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
            strcat(rv, "http://");
            strcat(rv, &url[ws_len]);
        } else if (strncmp(url, "wss://", wss_len) == 0) {
            strcat(rv, "https://");
            strcat(rv, &url[wss_len]);
        } else {
            strcat(rv, url);
        }
    }

    return rv;
}


char* cws_strdup(const char *s)
{
    char *rv = NULL;;

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
    char *rv = NULL;;

    if (s) {
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
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);
    char *p;

    p = (char*) malloc( l1 + l2 + 1 );
    if (p) {
        memcpy(p, s1, l1);
        memcpy(&p[l1], s2, l2);
        p[l1 + l2 + 1] = '\0';
    }

    return p;
}

void cws_hton(void *mem, uint8_t len)
{
#if __BYTE_ORDER__ != __BIG_ENDIAN
    uint8_t *bytes;
    uint8_t i, mid;

    if (len % 2) return;

    mid = len / 2;
    bytes = mem;
    for (i = 0; i < mid; i++) {
        uint8_t tmp = bytes[i];
        bytes[i] = bytes[len - i - 1];
        bytes[len - i - 1] = tmp;
    }
#endif
}


void cws_ntoh(void *mem, uint8_t len)
{
#if __BYTE_ORDER__ != __BIG_ENDIAN
    uint8_t *bytes;
    uint8_t i, mid;

    if (len % 2) return;

    mid = len / 2;
    bytes = mem;
    for (i = 0; i < mid; i++) {
        uint8_t tmp = bytes[i];
        bytes[i] = bytes[len - i - 1];
        bytes[len - i - 1] = tmp;
    }
#endif
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */
