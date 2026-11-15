/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * lock.h
 * Per-radio claim locks.
 */

#ifndef WLNK_LOCK_H
#define WLNK_LOCK_H

#include "radio.h"

/**
 * @brief Take a radio's lock, creating the run directory when missing.
 *
 * Fails rather than waits when another claim holds it. The kernel drops the
 * lock when its holder exits, so a crashed claim never keeps a radio.
 *
 * @param r The radio.
 * @param fd Receives the lock's descriptor; -1 on failure.
 * @return WLNK_OK; WLNK_E_BUSY when another claim holds it, its pid in the
 * message when known; WLNK_E_IO when the lock is not a plain file.
 */
int wlnk_lock_take(const wlnk_radio_t *r, int *fd);

/**
 * @brief Release a lock taken with wlnk_lock_take().
 * @param fd The lock's descriptor, -1 on return. A -1 is left alone.
 */
void wlnk_lock_drop(int *fd);

#endif /* WLNK_LOCK_H */
