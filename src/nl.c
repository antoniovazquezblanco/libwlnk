/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl.c
 * Netlink transport over libnl.
 */

#include <netlink/errno.h>
#include "err.h"

#include "nl.h"

int wlnk_nle_status(int nle)
{
    if (nle >= 0)
        return WLNK_OK;

    switch (nle) {
    case -NLE_INTR:
        return WLNK_E_INTERRUPTED;
    case -NLE_NOMEM:
        return WLNK_E_NO_MEMORY;
    case -NLE_PERM:
    case -NLE_NOACCESS:
        return WLNK_E_PERM;
    case -NLE_OBJ_NOTFOUND:
    case -NLE_NODEV:
        return WLNK_E_NOT_FOUND;
    case -NLE_OPNOTSUPP:
        return WLNK_E_UNSUPPORTED;
    case -NLE_INVAL:
    case -NLE_RANGE:
        return WLNK_E_INVALID_ARG;
    default:
        return WLNK_E_IO;
    }
}
