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

/** Size of the detail message, including the terminating NUL. */
#define WLNK_ERR_MSG_MAX 96

/**
 * @brief Success: clear this thread's detail.
 * @return WLNK_OK.
 */
int wlnk_ok(void);

/**
 * @brief Failure: record this thread's detail.
 * @param status Status to return.
 * @param syserrno The failing call's errno, or 0.
 * @param msg Detail message, or NULL; truncated to fit.
 * @return @p status.
 */
int wlnk_fail(int status, int syserrno, const char *msg);

/**
 * @brief Failure of a system call that set errno.
 *
 * As wlnk_fail(), with the message made of @p what, ": " and the system's
 * text for @p e.
 *
 * @param status Status to return.
 * @param e The failing call's errno; 0 leaves the message as @p what alone.
 * @param what The step that failed.
 * @return @p status.
 */
int wlnk_fail_sys(int status, int e, const char *what);

/**
 * @brief Status for a failed file, socket or ioctl call.
 * @param e The failing call's errno.
 * @return WLNK_E_PERM when the system refused it (EPERM, EACCES), else
 * WLNK_E_IO.
 */
int wlnk_errno_status(int e);

/**
 * @brief The system's text for an errno; unlike strerror(), thread-safe.
 * @param e errno value.
 * @param buf Buffer to write the text into.
 * @param cap Size of @p buf.
 * @return @p buf, or "" when @p buf is NULL or @p cap is 0.
 */
const char *wlnk_errno_text(int e, char *buf, size_t cap);

#endif /* WLNK_ERR_INTERNAL_H */
