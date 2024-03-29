/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <string.h>

#include "sha1/sha.h"

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
int cws_sha1(const void *in, size_t len, void *out)
{
    int rv = -1;
    SHA1Context ctx;
    uint8_t md[SHA1HashSize];

    if ((shaSuccess == SHA1Reset(&ctx))
        && (shaSuccess == SHA1Input(&ctx, (const uint8_t *) in, (unsigned int) len))
        && (shaSuccess == SHA1Result(&ctx, md)))
    {
        memcpy(out, md, SHA1HashSize);
        rv = 0;
    }

    return rv;
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */
