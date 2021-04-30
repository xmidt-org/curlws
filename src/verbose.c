/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>

#include "verbose.h"

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
void verbose(CWS *priv, const char *format, ...)
{
    if (priv->cfg.verbose) {
        va_list args;

        va_start(args, format);
        vfprintf(priv->cfg.verbose_stream, format, args);
        va_end(args);
    }
}


void verbose_close(CWS *priv)
{
    if (!priv->close_state) {
        verbose(priv, "[ websocket connection state: active ]\n");
    } else if ((CLOSED|CLOSE_SENT|CLOSE_QUEUED|CLOSE_RECEIVED) == priv->close_state) {
        verbose(priv, "[ websocket connection state: (closed)   closed  sent  queued  received ]\n");
    } else {
        verbose(priv, "[ websocket connection state: (closing) %cclosed %csent %cqueued %creceived ]\n",
                (CLOSED         & priv->close_state) ? ' ' : '!',
                (CLOSE_SENT     & priv->close_state) ? ' ' : '!',
                (CLOSE_QUEUED   & priv->close_state) ? ' ' : '!',
                (CLOSE_RECEIVED & priv->close_state) ? ' ' : '!' );
    }
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */

