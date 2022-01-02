/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../src/utils.h"

#define SEEN_AND_NEXT(type)    \
    do {                       \
        type->seen++;          \
        if (0 != type->more) { \
            type = &type[1];   \
        } else {               \
            type = NULL;       \
        }                      \
    } while (0)


/*----------------------------------------------------------------------------*/
/*                                  Mock Header                               */
/*----------------------------------------------------------------------------*/

CURLcode header_init(CWS *priv)
{
    CU_ASSERT(NULL != priv);
    return CURLE_OK;
}

/*----------------------------------------------------------------------------*/
/*                                 Mock Receive                               */
/*----------------------------------------------------------------------------*/

CURLcode receive_init(CWS *priv)
{
    CU_ASSERT(NULL != priv);
    return CURLE_OK;
}

/*----------------------------------------------------------------------------*/
/*                                  Mock Send                                 */
/*----------------------------------------------------------------------------*/

CURLcode send_init(CWS *priv)
{
    CU_ASSERT(NULL != priv);
    return CURLE_OK;
}

void send_destroy(CWS *priv)
{
    CU_ASSERT(NULL != priv);
}

size_t send_get_memory_needed(size_t payload_size)
{
    return 100 + payload_size;
}


/*----------------------------------------------------------------------------*/
/*                              Mock Frame Sender                             */
/*----------------------------------------------------------------------------*/
struct mock_sender {
    int options;
    const char *data;
    size_t len;
    CWScode rv;
    int seen;
    int more;
};

CWScode sender(struct mock_sender **_mock, int options, const char *data, size_t len)
{
    struct mock_sender *mock = *_mock;
    if (mock) {
        CWScode rv;

        if (mock->options != options) {
            printf("Expected: %08x, Got: %08x\n", mock->options, options);
        }
        CU_ASSERT_FATAL(mock->options == options);

        if (mock->len != len) {
            printf("Expected: %ld, Got: %ld\n", mock->len, len);
        }
        CU_ASSERT_FATAL(mock->len == len);

        if (0 < len) {
            int cmp = memcmp(data, mock->data, len);

            if (0 != cmp) {
                printf("Expected: '%s', Got: '%s'\n", mock->data, data);
            }
            CU_ASSERT_FATAL(0 == cmp);
        }
        rv = mock->rv;

        mock->seen++;
        if (0 != mock->more) {
            *_mock = &mock[1];
        } else {
            *_mock = NULL;
        }

        return rv;
    }

    return CWSE_OK;
}

static struct mock_sender *__frame_sender_control = NULL;
CWScode frame_sender_control(CWS *priv, int options, const void *data, size_t len)
{
    CU_ASSERT(NULL != priv);
    return sender(&__frame_sender_control, options, data, len);
}


static struct mock_sender *__frame_sender_data = NULL;
CWScode frame_sender_data(CWS *priv, int options, const void *data, size_t len)
{
    CU_ASSERT(NULL != priv);
    return sender(&__frame_sender_data, options, data, len);
}


/*----------------------------------------------------------------------------*/
/*                              Mock Data Block                               */
/*----------------------------------------------------------------------------*/
static struct mock_sender *__data_block_sender = NULL;
CWScode data_block_sender(CWS *priv, int options, const void *data, size_t len)
{
    CU_ASSERT(NULL != priv);
    return sender(&__data_block_sender, options, data, len);
}

/*----------------------------------------------------------------------------*/
/*                                Mock Random                                 */
/*----------------------------------------------------------------------------*/


void cws_random(CWS *priv, void *buffer, size_t len)
{
    uint8_t *bytes = buffer;
    size_t i;

    (void) priv;

    /* Always use the same 4 random numebrs */
    srand(1000);

    /* Note that this does NOT need to be a crypto level randomization function
     * but is simply used to prevent intermediary caches from causing issues. */
    for (i = 0; i < len; i++) {
        bytes[i] = (0x0ff & rand());
    }
}


/*----------------------------------------------------------------------------*/
/*                                 Mock Curl                                  */
/*----------------------------------------------------------------------------*/
#undef curl_easy_getinfo
#undef curl_easy_setopt

static bool __curl_info_test_value = false;
static curl_version_info_data __curl_info;
curl_version_info_data *curl_version_info(CURLversion v)
{
    (void) v;

    if (!__curl_info_test_value) {
        __curl_info.version_num = 0x073202;
        __curl_info.version     = "7.50.2";
    }

    return &__curl_info;
}


struct mock_curl_easy_init {
    const char *rv;
    int seen;
    int more;
};
static struct mock_curl_easy_init *__curl_easy_init = NULL;
CURL *curl_easy_init()
{
    CURL *rv = NULL;

    if (__curl_easy_init) {
        rv = (CURL *) __curl_easy_init->rv;
        SEEN_AND_NEXT(__curl_easy_init);
    } else {
        /* Really don't care, just not NULL. */
        rv = (CURL *) 42;
    }

    return rv;
}


struct mock_curl_easy_pause {
    int bitmask;
    int seen;
    int more;
};
static struct mock_curl_easy_pause *__curl_easy_pause = NULL;
CURLcode curl_easy_pause(CURL *easy, int bitmask)
{
    CU_ASSERT(NULL != easy);

    if (__curl_easy_pause) {
        CU_ASSERT(__curl_easy_pause->bitmask == bitmask);
        SEEN_AND_NEXT(__curl_easy_pause);
    }
    return CURLE_OK;
}


struct curl_slist *__curl_easy_setopt = NULL;
CURLcode curl_easy_setopt(CURL *easy, CURLoption option, ...)
{
    va_list ap;
    char buf[256];
    int width            = 23;
    struct curl_slist *p = NULL;

    CU_ASSERT(NULL != easy);

    memset(buf, 0, sizeof(buf));

    va_start(ap, option);
    switch (option) {
        case CURLOPT_URL:
            snprintf(buf, sizeof(buf), "%-*s: %s", width, "CURLOPT_URL", va_arg(ap, const char *));
            break;
        case CURLOPT_FOLLOWLOCATION:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_FOLLOWLOCATION", va_arg(ap, long));
            break;
        case CURLOPT_MAXREDIRS:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_MAXREDIRS", va_arg(ap, long));
            break;
        case CURLOPT_IPRESOLVE:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_IPRESOLVE", va_arg(ap, long));
            break;
        case CURLOPT_INTERFACE:
            snprintf(buf, sizeof(buf), "%-*s: %s", width, "CURLOPT_INTERFACE", va_arg(ap, const char *));
            break;
        case CURLOPT_SSLVERSION:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_SSLVERSION", va_arg(ap, long));
            break;
        case CURLOPT_SSL_VERIFYPEER:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_SSL_VERIFYPEER", va_arg(ap, long));
            break;
        case CURLOPT_SSL_VERIFYHOST:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_SSL_VERIFYHOST", va_arg(ap, long));
            break;
        case CURLOPT_VERBOSE:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_VERBOSE", va_arg(ap, long));
            break;
        case CURLOPT_HTTP_VERSION:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_HTTP_VERSION", va_arg(ap, long));
            break;
        case CURLOPT_UPLOAD:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_UPLOAD", va_arg(ap, long));
            break;
        case CURLOPT_CUSTOMREQUEST:
            snprintf(buf, sizeof(buf), "%-*s: %s", width, "CURLOPT_CUSTOMREQUEST", va_arg(ap, const char *));
            break;
        case CURLOPT_FORBID_REUSE:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_FORBID_REUSE", va_arg(ap, long));
            break;
        case CURLOPT_FRESH_CONNECT:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_FRESH_CONNECT", va_arg(ap, long));
            break;
        case CURLOPT_STDERR:
            snprintf(buf, sizeof(buf), "%-*s: %p", width, "CURLOPT_STDERR", (void *) va_arg(ap, FILE *));
            break;
        case CURLOPT_HTTPHEADER: /* pointer to headers */
            p = va_arg(ap, struct curl_slist *);
            break;
        default:
            snprintf(buf, sizeof(buf), "%-*s: %ld", width, "CURLOPT_unknown", (long) option);
            break;
    }
    va_end(ap);

    if (p) {
        while (p) {
            __curl_easy_setopt = curl_slist_append(__curl_easy_setopt, p->data);
            p                  = p->next;
        }
    } else {
        __curl_easy_setopt = curl_slist_append(__curl_easy_setopt, buf);
    }

    return CURLE_OK;
}


#if 0
CURLcode curl_easy_getinfo(CURL *easy, CURLINFO info, ... )
{
    return CURLE_OK;
}
#endif

void curl_easy_cleanup(CURL *easy)
{
    (void) easy;
}


CURLMcode curl_multi_add_handle(CURL *easy, CURLM *multi_handle)
{
    (void) easy;
    (void) multi_handle;
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle(CURL *easy, CURLM *multi_handle)
{
    (void) easy;
    (void) multi_handle;
    return CURLM_OK;
}


/* These basically work like the real thing. */
struct curl_slist *curl_slist_append(struct curl_slist *list, const char *s)
{
    struct curl_slist *last;
    struct curl_slist *new;

    new = malloc(sizeof(struct curl_slist));
    if (!new) {
        return NULL;
    }

    new->next = NULL;
    new->data = cws_strdup(s);
    if (!new->data) {
        free(new);
        return NULL;
    }

    /* Empty starting list. */
    if (!list) {
        return new;
    }

    last = list;
    while (last->next) {
        last = last->next;
    }
    last->next = new;

    return list;
}

void curl_slist_free_all(struct curl_slist *list)
{
    while (list) {
        struct curl_slist *tmp;

        tmp  = list;
        list = list->next;
        free(tmp->data);
        free(tmp);
    }
}

void curl_slist_print(struct curl_slist *list)
{
    while (list) {
        printf("%s\n", list->data);
        list = list->next;
    }
}


void curl_slist_compare(struct curl_slist *list, const char *s[])
{
    while (list) {
        int rv = strcmp(list->data, *s);
        if (0 != rv) {
            fprintf(stderr, "\nWanted: %s\n   Got: %s\n", *s, list->data);
        }
        CU_ASSERT_FATAL(0 == rv);
        list = list->next;
        s++;
    }
}
