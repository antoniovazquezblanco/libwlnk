/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * lock.c
 * Per-radio claim locks.
 *
 * A radio is held by whoever holds the flock on <run dir>/<radio id>.lock.
 * The kernel drops a flock when its holder exits, so a held lock always
 * means a live claim. The pid written in the file is only a hint for the
 * message. Lock files are never deleted: unlinking one races with a
 * claimer that has just opened it.
 *
 * Other users may write to the run directory, so nothing in it is trusted:
 * the lock is opened relative to the directory, never through a symlink,
 * and must be a plain file before anything is written to it.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include "err.h"

#include "err_internal.h"
#include "lock.h"
#include "rundir.h"

/** Message for a lock path that is not a plain file. */
#define UNSAFE_LOCK "lock: unsafe lock file (not a plain file)"

/**
 * @brief The pid a lock's holder wrote in it.
 * @param fd The lock.
 * @return The pid; 0 when there is none to read.
 */
static uint32_t holder_pid(int fd)
{
    char buf[16];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    long pid = strtol(buf, NULL, 10);
    return pid > 0 && pid <= INT32_MAX ? (uint32_t)pid : 0;
}

int wlnk_lock_take(const wlnk_radio_t *r, int *fd)
{
    if (fd != NULL)
        *fd = -1;
    if (r == NULL || fd == NULL || r->id[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "lock: no radio");

    int dfd = -1;
    int rc = wlnk_rundir_open(true, &dfd);
    if (rc != WLNK_OK)
        return rc;

    /* The identity is safe as a file name, so this stays in the directory. */
    char name[WLNK_RADIO_ID_MAX + 8];
    snprintf(name, sizeof(name), "%s.lock", r->id);

    /* Another user's lock cannot be opened for writing, but flock() works
     * on a read-only descriptor: only the pid hint is lost. O_NONBLOCK
     * keeps a FIFO planted here from blocking the open. */
    int flags = O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
    bool writable = true;
    int lock = openat(dfd, name, flags | O_RDWR | O_CREAT, 0644);
    int e = errno; /* the first failure is the one worth reporting */
    if (lock < 0 && e == EACCES) {
        writable = false;
        lock = openat(dfd, name, flags | O_RDONLY);
    }
    close(dfd);
    if (lock < 0) {
        if (e == ELOOP)
            return wlnk_fail(WLNK_E_IO, 0, UNSAFE_LOCK);
        return wlnk_fail_sys(wlnk_errno_status(e), e, "lock: open");
    }

    /* Before anything is written: a hard link would point the write at a
     * file the directory's writer chose. */
    struct stat st;
    if (fstat(lock, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
        close(lock);
        return wlnk_fail(WLNK_E_IO, 0, UNSAFE_LOCK);
    }

    if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
        e = errno;
        uint32_t pid = holder_pid(lock);
        close(lock);
        if (e != EWOULDBLOCK)
            return wlnk_fail_sys(WLNK_E_IO, e, "lock: take");
        char msg[WLNK_ERR_MSG_MAX];
        if (pid != 0)
            snprintf(msg, sizeof(msg), "%s: radio held by pid %u", r->phy,
                     (unsigned)pid);
        else
            snprintf(msg, sizeof(msg), "%s: radio held by another claim",
                     r->phy);
        return wlnk_fail(WLNK_E_BUSY, 0, msg);
    }

    /* Only a hint, so failing to leave it is not an error. */
    if (writable && ftruncate(lock, 0) == 0)
        dprintf(lock, "%ld\n", (long)getpid());
    *fd = lock;
    return wlnk_ok();
}

void wlnk_lock_drop(int *fd)
{
    if (fd == NULL || *fd < 0)
        return;
    /* Unlocked first: a forked child may share the descriptor, and closing
     * ours alone would leave the radio held. */
    (void)flock(*fd, LOCK_UN);
    close(*fd);
    *fd = -1;
}
