/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err.c
 * Status codes.
 */

#include "err.h"

const char *wlnk_strerr(int status)
{
    switch (status) {
    case WLNK_OK:
        return "success";
    case WLNK_E_PERM:
        return "insufficient privileges";
    case WLNK_E_BUSY:
        return "radio is busy";
    case WLNK_E_UNSUPPORTED:
        return "operation not supported";
    case WLNK_E_INVALID_ARG:
        return "invalid argument";
    case WLNK_E_NO_MEMORY:
        return "out of memory";
    case WLNK_E_NOT_FOUND:
        return "not found";
    case WLNK_E_IO:
        return "I/O error";
    case WLNK_E_TIMEOUT:
        return "operation timed out";
    case WLNK_E_RF_KILL:
        return "radio is blocked (rfkill)";
    case WLNK_E_VIF_LIMIT:
        return "no room for another interface on this radio";
    case WLNK_E_STALE_STATE:
        return "an earlier claim was left unfinished";
    case WLNK_E_INTERRUPTED:
        return "interrupted by a signal";
    default:
        return "unknown status";
    }
}
