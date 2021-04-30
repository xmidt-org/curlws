/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "curlws.h"
#include "data_block_sender.h"
#include "frame_senders.h"
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
CWScode data_block_sender(CWS *priv, int options, const void *data, size_t len)
{
    const uint8_t *buffer = data;

    switch (options) {
        case CWS_BINARY:
        case CWS_TEXT:
            break;
        default:
            return CWSE_INVALID_OPTIONS;
    }

    if (priv->close_state) {
        return CWSE_CLOSED_CONNECTION;
    }

    options |= CWS_FIRST;
    if (!buffer || !len) {
        options |= CWS_LAST;
        return frame_sender_data(priv, options, NULL, 0);
    }

    while (priv->cfg.max_payload_size < len) {
        CWScode rv;

        rv = frame_sender_data(priv, options, buffer, priv->cfg.max_payload_size);
        if (CWSE_OK != rv) {    /* Should only fail if we ran out of memory */
            return rv;
        }

        options = CWS_CONT;
        len -= priv->cfg.max_payload_size;
        buffer += priv->cfg.max_payload_size;
    }

    options |= CWS_LAST;
    return frame_sender_data(priv, options, buffer, len);
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */

