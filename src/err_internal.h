/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err_internal.h
 * Recording the error detail. Not installed.
 *
 * Every wlnk call that returns a status returns through wlnk_ok() or
 * wlnk_fail(), so the calling thread's detail always describes its last
 * such call.
 */

#ifndef WLNK_ERR_INTERNAL_H
#define WLNK_ERR_INTERNAL_H

#include <stddef.h>

/* Size of the detail message, including the terminating NUL. */
#define WLNK_ERR_MSG_MAX 96

/* Success: clear this thread's detail and return WLNK_OK. */
int wlnk_ok(void);

/* Failure: record syserrno (the failing call's errno or 0) and msg (may be
 * NULL; truncated to fit) as this thread's detail and return status. */
int wlnk_fail(int status, int syserrno, const char *msg);

/* Failure of a system call that set errno e: as wlnk_fail(), with msg
 * reading "<what>: <system text for e>". e == 0 leaves msg as what alone. */
int wlnk_fail_sys(int status, int e, const char *what);

/* Status for a failed file, socket or ioctl call: WLNK_E_PERM when the
 * system refused it (EPERM, EACCES), WLNK_E_IO otherwise. */
int wlnk_errno_status(int e);

/* The system's text for errno e, written into buf; unlike strerror(), safe
 * from any thread. Returns buf. */
const char *wlnk_errno_text(int e, char *buf, size_t cap);

#endif /* WLNK_ERR_INTERNAL_H */
