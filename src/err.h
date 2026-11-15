/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err.h
 * Status codes.
 */

#ifndef WLNK_ERR_H
#define WLNK_ERR_H

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes returned by wlnk calls.
 */
enum wlnk_status {
    /** Success. */
    WLNK_OK = 0,
    /** Insufficient permissions. */
    WLNK_E_PERM = 1,
    /** The radio is held by a claim or kept busy by another interface. */
    WLNK_E_BUSY = 2,
    /** The radio or this build cannot do what was asked. */
    WLNK_E_UNSUPPORTED = 3,
    /** An argument is invalid. */
    WLNK_E_INVALID_ARG = 4,
    /** Out of memory. */
    WLNK_E_NO_MEMORY = 5,
    /** No such interface or radio, or no nl80211 in the kernel. */
    WLNK_E_NOT_FOUND = 6,
    /** A system call failed. */
    WLNK_E_IO = 7,
    /** The operation timed out. */
    WLNK_E_TIMEOUT = 8,
    /** The radio is hard or soft blocked (rfkill). */
    WLNK_E_RF_KILL = 9,
    /** No room for another interface on the radio. */
    WLNK_E_VIF_LIMIT = 10,
    /** An earlier claim was left unfinished by a crash or failed release. */
    WLNK_E_STALE_STATE = 11,
    /** A wait was interrupted by a signal. */
    WLNK_E_INTERRUPTED = 12
};

/**
 * @brief Stable, human-readable message for a status code.
 * @param status Status code.
 * @return Static message; "unknown status" for unknown codes.
 */
WLNK_API const char *wlnk_strerr(int status);

#ifdef __cplusplus
}
#endif

#endif /* WLNK_ERR_H */
