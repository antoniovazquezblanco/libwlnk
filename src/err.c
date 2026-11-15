/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err.c
 * Status codes and error detail.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "err.h"

#include "err_internal.h"

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

/* This thread's detail: the outcome of its last call that returns a status. */
static _Thread_local struct {
    int syserrno;
    char msg[WLNK_ERR_MSG_MAX];
} last;

int wlnk_err_syserrno(void)
{
    return last.syserrno;
}

const char *wlnk_err_msg(void)
{
    return last.msg;
}

int wlnk_ok(void)
{
    memset(&last, 0, sizeof(last));
    return WLNK_OK;
}

int wlnk_fail(int status, int syserrno, const char *msg)
{
    last.syserrno = syserrno;
    snprintf(last.msg, sizeof(last.msg), "%s", msg != NULL ? msg : "");
    return status;
}

/**
 * @brief Append to this thread's detail message, truncated to fit.
 * @param s Text to append.
 */
static void msg_append(const char *s)
{
    size_t used = strlen(last.msg);
    size_t n = strlen(s);
    if (n > sizeof(last.msg) - 1 - used)
        n = sizeof(last.msg) - 1 - used;
    memcpy(last.msg + used, s, n);
    last.msg[used + n] = '\0';
}

int wlnk_fail_sys(int status, int e, const char *what)
{
    wlnk_fail(status, e, what);
    if (e == 0)
        return status;
    /* After the step wlnk_fail() put there; truncated to fit like it. */
    char text[WLNK_ERR_MSG_MAX];
    wlnk_errno_text(e, text, sizeof(text));
    if (last.msg[0] != '\0')
        msg_append(": ");
    msg_append(text);
    return status;
}

int wlnk_errno_status(int e)
{
    return (e == EPERM || e == EACCES) ? WLNK_E_PERM : WLNK_E_IO;
}

const char *wlnk_errno_text(int e, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0)
        return "";
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    /* The GNU strerror_r() (glibc, and uClibc-ng, which defines __GLIBC__
     * too) returns the text, in buf or in static storage. */
    const char *s = strerror_r(e, buf, cap);
    if (s != buf)
        snprintf(buf, cap, "%s", s);
#else
    /* The POSIX one (musl and the rest) fills buf or fails. */
    if (strerror_r(e, buf, cap) != 0)
        snprintf(buf, cap, "error %d", e);
#endif
    return buf;
}
