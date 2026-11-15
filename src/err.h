/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err.h
 * Status codes and error detail.
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
    /** A system call failed. Its errno is wlnk_err_syserrno(). */
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

/**
 * @brief errno behind the last failed wlnk call on this thread.
 *
 * Every wlnk call that returns a status records its outcome for the calling
 * thread: cleared on success, filled on failure. Read it before the next
 * such call on the same thread.
 *
 * @return The failing system call's errno, or 0 when there is none.
 */
WLNK_API int wlnk_err_syserrno(void);

/**
 * @brief Detail of the last failed wlnk call on this thread.
 *
 * Recorded as for wlnk_err_syserrno(). Not stable: match on the status.
 *
 * @return Human-readable detail, empty when there is none. Valid until the next
 * wlnk call that returns a status on this thread.
 */
WLNK_API const char *wlnk_err_msg(void);

#ifdef __cplusplus
}
#endif

#endif /* WLNK_ERR_H */
