/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * rundir.c
 * The run directory, where claims keep their locks and records.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "err.h"

#include "err_internal.h"
#include "rundir.h"

/* Where it is. Only the tests set it, to a directory of their own. */
#ifndef WLNK_RUNDIR
#define WLNK_RUNDIR "/run/wlnk"
#endif

int wlnk_rundir_open(bool create, int *dfd)
{
    *dfd = -1;
    if (create && mkdir(WLNK_RUNDIR, 0755) != 0 && errno != EEXIST) {
        int e = errno;
        return wlnk_fail_sys(wlnk_errno_status(e), e, "run directory: create");
    }
    *dfd = open(WLNK_RUNDIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (*dfd >= 0)
        return wlnk_ok();
    int e = errno;
    if (e == ENOENT && !create)
        return wlnk_ok();
    if (e == ELOOP || e == ENOTDIR)
        return wlnk_fail(WLNK_E_IO, 0,
                         "run directory: unsafe (not a plain directory)");
    return wlnk_fail_sys(wlnk_errno_status(e), e, "run directory: open");
}

bool wlnk_rundir_is_plain(int fd)
{
    struct stat st;
    return fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1;
}
