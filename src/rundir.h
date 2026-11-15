/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * rundir.h
 * The run directory, where claims keep their locks and records.
 */

#ifndef WLNK_RUNDIR_H
#define WLNK_RUNDIR_H

#include <stdbool.h>

/**
 * @brief Open the run directory.
 *
 * Other users may write to it, so it is never followed through a symlink.
 * A deployment that wants it group-writable creates it itself (a tmpfiles.d
 * line); this only makes sure there is one.
 *
 * @param create Create it when missing. Otherwise a missing one is no
 * error, and @p dfd is -1.
 * @param dfd Receives its descriptor, or -1.
 * @return WLNK_OK; WLNK_E_IO when it is not a plain directory.
 */
int wlnk_rundir_open(bool create, int *dfd);

#endif /* WLNK_RUNDIR_H */
