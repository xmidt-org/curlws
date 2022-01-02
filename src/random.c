/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#define _XOPEN_SOURCE 600

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>

#include "internal.h"

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
void cws_random(CWS *priv, void *buffer, size_t len)
{
    uint8_t *bytes    = buffer;
    static int seeded = 0;

    IGNORE_UNUSED(priv);

    if (0 == seeded) {
        struct timespec ts;

        if (0 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
            srandom(((int) ts.tv_nsec) ^ ((int) ts.tv_sec));
            seeded = 1;
        }
    }

    /* Note that this does NOT need to be a crypto level randomization function
     * but is simply used to prevent intermediary caches from causing issues. */
    for (size_t i = 0; i < len; i++) {
        bytes[i] = (0x0ff & random());
    }
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */
